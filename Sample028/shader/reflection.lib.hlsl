#include "common.hlsli"
#include "payload.hlsli"
#include "colorspace.hlsli"
#include "sh.hlsli"
#include "reflection_ray.hlsli"

#define RayTMax			10000.0
#ifndef ENABLE_RAY_BINNING
#	define	ENABLE_RAY_BINNING		0
#endif
#ifndef ENABLE_SER
#	define	ENABLE_SER				0
#endif

#if ENABLE_SER
#	define NV_SHADER_EXTN_SLOT u7
#	define NV_HITOBJECT_USE_MACRO_API
#	include "../../External/NVAPI/nvHLSLExtns.h"
#endif

// global
ConstantBuffer<SceneCB>				cbScene			: register(b0);
ConstantBuffer<LightCB>				cbLight			: register(b1);
ConstantBuffer<MaterialCB>			cbMaterial		: register(b2);

RaytracingAccelerationStructure		TLAS			: register(t0, space0);
Texture2D							texGBuffer0		: register(t1);
Texture2D							texGBuffer1		: register(t2);
Texture2D							texGBuffer2		: register(t3);
Texture2D<float>					texDepth		: register(t4);
StructuredBuffer<PointLightPos>		rLightPosBuffer		: register(t5);
StructuredBuffer<PointLightColor>	rLightColorBuffer	: register(t6);
Texture2D							texHDRI			: register(t7);
StructuredBuffer<SH9Color>			rSH				: register(t8);
#if ENABLE_RAY_BINNING
StructuredBuffer<RayData>			rRayData		: register(t9);
#else
Texture2D<float2>					texBlueNoise	: register(t9);
#endif

SamplerState						samHDRI			: register(s0);

RWTexture2D<float4>					RenderTarget	: register(u0);


float2 CubemapDirToPanoramaUV(float3 dir)
{
	return float2(
		0.5f + 0.5f * atan2(dir.z, dir.x) / PI,
		acos(dir.y) / PI);
}

float3 SampleHDRI(float3 dir)
{
	float2 panoramaUV = CubemapDirToPanoramaUV(dir);
	return texHDRI.SampleLevel(samHDRI, panoramaUV, 0).rgb;
}

float PointLightAttenuation(float d, float r)
{
	const float SourceRadius = 0.01;
	float dd = d * d;
	float rr = SourceRadius * SourceRadius;
	float attn = 2.0 / (dd + rr + d * sqrt(dd + rr));
	float s = 1.0 - smoothstep(r * 0.9, r, d);
	return attn * s;
}

float3 Lighting(MaterialParam matParam, float3 worldPos, float3 viewDirInWS)
{
	float3 normalInWS = matParam.normal;
	float3 diffuseColor = matParam.baseColor.rgb * (1 - matParam.metallic);
	float3 specularColor = 0.04 * (1 - matParam.metallic) + matParam.baseColor.rgb * matParam.metallic;

	float3 dlightColor = cbLight.lightColor.rgb;
	float3 skyColor = EvalSH9Cosine(normalInWS, rSH[0]);

	if (cbScene.renderColorSpace != 0)
	{
		diffuseColor = Rec709ToRec2020(diffuseColor);
		specularColor = Rec709ToRec2020(specularColor);
	}
	if (cbScene.renderColorSpace == 1)
	{
		dlightColor = Rec709ToRec2020(dlightColor);
		skyColor = Rec709ToRec2020(skyColor);
	}

	// shadow ray.
	float3 direction = -cbLight.lightDir.xyz;
	float3 origin = worldPos.xyz + normalInWS * 1e-2;
	uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
	RayDesc rayDesc = { origin, 0.0, direction, RayTMax };
	HitPayload payload;
	payload.hitT = -1;
	TraceRay(TLAS, rayFlags, ~0, kShadowContribution, kGeometricContributionMult, 0, rayDesc, payload);

	// directional light.
	float3 directColor = BrdfGGX(diffuseColor, specularColor, matParam.roughness, normalInWS, -cbLight.lightDir.rgb, -viewDirInWS) * dlightColor;
	float directShadow = payload.hitT < 0 ? 1.0 : 0.0;
	float3 finalColor = directColor * directShadow + diffuseColor * skyColor * cbLight.skyPower;

#if ENABLE_POINT_LIGHTS
	// point lights.
	for (uint plIdx = 0; plIdx < 128; plIdx++)
	{
		PointLightPos lightPos = rLightPosBuffer[plIdx];
		float3 lightDir = lightPos.posAndRadius.xyz - worldPos;
		float lightLen = length(lightDir);
		if (lightLen > lightPos.posAndRadius.w)
			continue;

		PointLightColor lightColor = rLightColorBuffer[plIdx];
		float3 plightColor = lightColor.color.rgb;

		if (cbScene.renderColorSpace == 1)
		{
			plightColor = Rec709ToRec2020(plightColor);
		}

		lightDir *= 1.0 / (lightLen + Epsilon);

		directColor = BrdfGGX(diffuseColor, specularColor, matParam.roughness, normalInWS, lightDir, -viewDirInWS)
			* plightColor
			* PointLightAttenuation(lightLen, lightPos.posAndRadius.w);
		finalColor += directColor;
	}
#endif

	return finalColor + matParam.emissive;
}

[shader("raygeneration")]
void ReflectionRGS()
{
#if ENABLE_RAY_BINNING
	RayData Ray = rRayData[DispatchRaysIndex().x];
	uint2 PixelPos = uint2(Ray.packedPixelPos & 0xffff, (Ray.packedPixelPos >> 16) & 0xffff);
	if (Ray.flag == 0x0)
	{
		if (all(PixelPos < uint2(cbScene.screenInfo.zw)))
		{
			RenderTarget[PixelPos] = (0).xxxx;
		}
		return;
	}
#else
	uint2 PixelPos = DispatchRaysIndex().xy;
	float depth = texDepth[PixelPos];
	if (depth >= 1.0)
	{
		if (all(PixelPos < uint2(cbScene.screenInfo.zw)))
		{
			RenderTarget[PixelPos] = (0).xxxx;
		}
		return;
	}
#endif

	// get gbuffer.
	float4 gin0 = texGBuffer0[PixelPos];
	float4 gin1 = texGBuffer1[PixelPos];
	float4 gin2 = texGBuffer2[PixelPos];
	GBuffer gb = DecodeGBuffer(gin0, gin1, gin2);

#if !ENABLE_RAY_BINNING
	// get world position.
	float2 screenPos = ((float2)PixelPos + 0.5) / cbScene.screenInfo.zw;
	float2 clipSpacePos = screenPos * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	// get mirror reflection ray.
	// TODO: implement rough surface reflection ray.
	float3 normalInWS = ConvertVectorTangentToWorld(float3(0, 0, 1), gb.worldQuat);
	float3 viewDir = normalize(worldPos.xyz - cbScene.camPos.xyz);
#if 0
	float3 reflectDir = reflect(viewDir, normalInWS);
#else
	float2 noiseU = frac(texBlueNoise[PixelPos % 128] + (cbScene.frameIndex & 0xff) * kGoldenRatio);
	float3 reflectDir = SampleReflectionRay(viewDir, gb.worldQuat, gb.roughness, noiseU);
#endif
	RayData Ray = (RayData)0;
	Ray.origin = worldPos + normalInWS * 0.01;
	Ray.direction = reflectDir;
#endif

	uint rayFlags = RAY_FLAG_CULL_NON_OPAQUE;
	//uint rayFlags = RAY_FLAG_NONE;
	RayDesc rayDesc = { Ray.origin, 0.0, Ray.direction, RayTMax };
	MaterialPayload payload;
	payload.hitT = -1;
#if !ENABLE_SER
	TraceRay(TLAS, rayFlags, ~0, kMaterialContribution, kGeometricContributionMult, 0, rayDesc, payload);
#else
	// trace ray and get hit object.
	NvHitObject nvHitObj;
	NvTraceRayHitObject(TLAS, rayFlags, ~0, kMaterialContribution, kGeometricContributionMult, 0, rayDesc, payload, nvHitObj);

	// reorder thread according to hit object.
	NvReorderThread(nvHitObj);

	// invoke hit object.
	NvInvokeHitObject(TLAS, nvHitObj, payload);
#endif

	if (payload.hitT >= 0)
	{
		// decode material.
		MaterialParam matParam;
		DecodeMaterialPayload(payload, matParam);
		float3 worldPos = Ray.origin + Ray.direction * payload.hitT;

		// lighting.
		float3 finalColor = Lighting(matParam, worldPos, normalize(Ray.direction));
		RenderTarget[PixelPos] = float4(finalColor, 1);
	}
	else
	{
		// sample hdri.
		float3 hdri = SampleHDRI(Ray.direction);
		RenderTarget[PixelPos] = float4(hdri, 1);
	}
}

[shader("miss")]
void ReflectionMS(inout MaterialPayload payload : SV_RayPayload)
{
	// not implement.
}

// EOF
