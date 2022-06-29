#define RTXGI_DDGI_PROBE_RELOCATION			1
#define RTXGI_DDGI_PROBE_STATE_CLASSIFIER	1

#include "rtxgi.hlsli"
#include "common.hlsli"
#include "payload.hlsli"
#include "colorspace.hlsli"
#include "sh.hlsli"

#define kShadowRayTMax			10000.0

// global
RaytracingAccelerationStructure				TLAS					: register(t0, space0);
StructuredBuffer<DDGIVolumeDescGPUPacked>	DDGIVolumeDesc			: register(t1);
Texture2D<float4>							DDGIProbeIrradiance		: register(t2);
Texture2D<float4>							DDGIProbeDistance		: register(t3);
Texture2D<float4>							DDGIProbeData			: register(t4);
StructuredBuffer<PointLightPos>				rLightPosBuffer			: register(t5);
StructuredBuffer<PointLightColor>			rLightColorBuffer		: register(t6);
Texture2D									texHDRI					: register(t7);

RWTexture2D<float4>							DDGIRayData				: register(u0);

ConstantBuffer<SceneCB>						cbScene					: register(b0);
ConstantBuffer<LightCB>						cbLight					: register(b1);
ConstantBuffer<MaterialCB>					cbMaterial				: register(b2);
ConstantBuffer<DDGIConstants>				cbDDGIConst				: register(b3);

SamplerState								BilinearWrapSampler		: register(s0);


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

	// shadow ray.
	float3 direction = -cbLight.lightDir.xyz;
	float3 origin = worldPos.xyz + normalInWS * 1e-2;
	uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
	RayDesc rayDesc = { origin, 0.0, direction, kShadowRayTMax };
	HitPayload payload;
	payload.hitT = -1;
	TraceRay(TLAS, rayFlags, ~0, kShadowContribution, kGeometricContributionMult, 0, rayDesc, payload);

	// directional light.
	float3 directColor = BrdfGGX(diffuseColor, specularColor, matParam.roughness, normalInWS, -cbLight.lightDir.rgb, -viewDirInWS) * dlightColor;
	float directShadow = payload.hitT < 0 ? 1.0 : 0.0;
	float3 finalColor = directColor * directShadow;

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

		lightDir *= 1.0 / (lightLen + Epsilon);

		directColor = BrdfGGX(diffuseColor, specularColor, matParam.roughness, normalInWS, lightDir, -viewDirInWS)
			* plightColor
			* PointLightAttenuation(lightLen, lightPos.posAndRadius.w);
		finalColor += directColor;
	}
#endif

	return finalColor + matParam.emissive;
}

float2 CubemapDirToPanoramaUV(float3 dir)
{
	return float2(
		0.5f + 0.5f * atan2(dir.z, dir.x) / PI,
		acos(dir.y) / PI);
}

float3 SampleHDRI(float3 dir)
{
	float2 panoramaUV = CubemapDirToPanoramaUV(dir);
	return texHDRI.SampleLevel(BilinearWrapSampler, panoramaUV, 0).rgb;
}

[shader("raygeneration")]
void ProbeLightingRGS()
{
	uint2 DispatchIndex = DispatchRaysIndex().xy;
	int rayIndex = DispatchIndex.x;						// index of ray within a probe
	int probeIndex = DispatchIndex.y;					// index of current probe

	uint volumeIndex = cbDDGIConst.volumeIndex;

	// get volume desc.
	DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumeDesc[volumeIndex]);

	// get probe grid coords.
	float3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

	// adjust probe index for scroll.
	probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

	// get probe state.
	float probeState = DDGILoadProbeState(probeIndex, DDGIProbeData, volume);

	// early out.
	if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE && rayIndex >= RTXGI_DDGI_NUM_FIXED_RAYS)
	{
		return;
	}

	// get probe's world position and normalized ray direction.
	float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, DDGIProbeData);
	float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, volume);

	// get output coords.
	uint2 texCoords = uint2(rayIndex, probeIndex);

	// trace probe ray.
	RayDesc ray;
	ray.Origin = probeWorldPosition;
	ray.Direction = probeRayDirection;
	ray.TMin = 0.0;
	ray.TMax = volume.probeMaxRayDistance;
	MaterialPayload payload = (MaterialPayload)0;
	payload.hitT = -1.0;
	TraceRay(TLAS, RAY_FLAG_NONE, ~0, kMaterialContribution, kGeometricContributionMult, 0, ray, payload);

	// ray miss.
	if (payload.hitT < 0.0)
	{
		float3 skyColor = SampleHDRI(probeRayDirection) * cbLight.skyPower;
		DDGIStoreProbeRayMiss(DDGIRayData, texCoords, volume, skyColor);
		return;
	}

	// decode material from payload.
	MaterialParam mat_param;
	DecodeMaterialPayload(payload, mat_param);

	// ray hit back face.
	if (mat_param.flag & kFlagBackFaceHit)
	{
		DDGIStoreProbeRayBackfaceHit(DDGIRayData, texCoords, volume, payload.hitT);
		return;
	}

	// early out: store ray hit distance only.
	if((volume.probeRelocationEnabled || volume.probeClassificationEnabled) && rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS)
	{
		DDGIStoreProbeRayFrontfaceHit(DDGIRayData, texCoords, volume, payload.hitT);
		return;
	}

	// lighting.
	float3 worldPos = ray.Origin + ray.Direction * payload.hitT;
	float3 diffuse = Lighting(mat_param, worldPos, normalize(ray.Direction));
	float3 diffuseColor = mat_param.baseColor.rgb * (1 - mat_param.metallic);

	// calc irradiance recursive.
	float3 irradiance = 0;
	{
		float3 surfaceBias = DDGIGetSurfaceBias(mat_param.normal, ray.Direction, volume);

		// setup ddgi resources.
		DDGIVolumeResources resources;
		resources.probeIrradiance = DDGIProbeIrradiance;
		resources.probeDistance = DDGIProbeDistance;
		resources.probeData = DDGIProbeData;
		resources.bilinearSampler = BilinearWrapSampler;

		// get volume blend weight
		float volumeBlendWeight = DDGIGetVolumeBlendWeight(worldPos, volume);

		if (volumeBlendWeight > 0.0)
		{
			// get irradiance.
			irradiance = DDGIGetVolumeIrradiance(
				worldPos,
				surfaceBias,
				mat_param.normal,
				volume,
				resources);

			// attenuation.
			irradiance *= volumeBlendWeight;
		}
	}

	// output.
	float3 radiance = diffuse + ((diffuseColor / PI) * irradiance);
	DDGIStoreProbeRayFrontfaceHit(DDGIRayData, texCoords, volume, radiance, payload.hitT);
}

[shader("miss")]
void ProbeLightingMS(inout MaterialPayload payload : SV_RayPayload)
{
	// not implement.
}

// EOF
