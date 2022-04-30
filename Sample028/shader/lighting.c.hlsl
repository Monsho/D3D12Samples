#include "common.hlsli"
#include "colorspace.hlsli"
#include "sh.hlsli"
#include "rtxgi.hlsli"

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);

Texture2D							texGBuffer0			: register(t0);
Texture2D							texGBuffer1			: register(t1);
Texture2D							texGBuffer2			: register(t2);
Texture2D<float>					texDepth			: register(t3);
Texture2D<float>					texScreenShadow		: register(t4);
StructuredBuffer<PointLightPos>		rLightPosBuffer		: register(t5);
StructuredBuffer<PointLightColor>	rLightColorBuffer	: register(t6);
StructuredBuffer<ClusterInfo>		rClusterInfo		: register(t7);
Texture2D							texHDRI				: register(t8);
StructuredBuffer<SH9Color>			rSH					: register(t9);
StructuredBuffer<DDGIVolumeDescGPUPacked>	DDGIVolumeDesc			: register(t10);
Texture2D<float4>							DDGIProbeIrradiance		: register(t11);
Texture2D<float4>							DDGIProbeDistance		: register(t12);
Texture2D<float4>							DDGIProbeData			: register(t13);

SamplerState						samHDRI				: register(s0);

RWTexture2D<float4>	rwOutput		: register(u0);

ClusterInfo GetCluster(float2 screenPos, float depth)
{
	float nearZ = cbScene.screenInfo.x;
	float farZ = cbScene.screenInfo.y;
	float viewDepth = ToViewDepth(depth, nearZ, farZ);
	uint z = uint(max(-viewDepth - nearZ, 0) * (float)CLUSTER_DIV_Z / (farZ - nearZ));
	uint2 xy = uint2(screenPos * (float)CLUSTER_DIV_XY);

	uint infoIndex = z * CLUSTER_DIV_XY * CLUSTER_DIV_XY + xy.y * CLUSTER_DIV_XY + xy.x;

	return rClusterInfo[infoIndex];
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

float3 Lighting(uint2 pixelPos, float depth)
{
	// get gbuffer.
	float4 gin0 = texGBuffer0[pixelPos];
	float4 gin1 = texGBuffer1[pixelPos];
	float4 gin2 = texGBuffer2[pixelPos];
	GBuffer gb = DecodeGBuffer(gin0, gin1, gin2);

	// get world position.
	float2 screenPos = ((float2)pixelPos + 0.5) / cbScene.screenInfo.zw;
	float2 clipSpacePos = screenPos * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	float3 viewDirInWS = normalize(worldPos.xyz - cbScene.camPos.xyz);
	float3 normalInWS = ConvertVectorTangentToWorld(float3(0, 0, 1), gb.worldQuat);
	float3 diffuseColor = gb.baseColor.rgb * (1 - gb.metallic);
	float3 specularColor = 0.04 * (1 - gb.metallic) + gb.baseColor.rgb * gb.metallic;

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

	// directional light.
	float3 directColor = BrdfGGX(diffuseColor, specularColor, gb.roughness, normalInWS, -cbLight.lightDir.rgb, -viewDirInWS) * dlightColor;
	float directShadow = texScreenShadow[pixelPos];
	float3 finalColor = directColor * directShadow;// + diffuseColor * skyColor * cbLight.skyPower;

#if ENABLE_POINT_LIGHTS
	// point lights.
	ClusterInfo cluster = GetCluster(screenPos, depth);
	for (uint flagIndex = 0; flagIndex < 4; flagIndex++)
	{
		uint flags = cluster.lightFlags[flagIndex];
		uint baseIndex = flagIndex * 32;
		while (flags)
		{
			uint index = firstbitlow(flags);
			flags &= ~(0x1 << index);

			PointLightPos lightPos = rLightPosBuffer[baseIndex + index];
			PointLightColor lightColor = rLightColorBuffer[baseIndex + index];
			float3 plightColor = lightColor.color.rgb;

			if (cbScene.renderColorSpace == 1)
			{
				plightColor = Rec709ToRec2020(plightColor);
			}

			float3 lightDir = lightPos.posAndRadius.xyz - worldPos.xyz;
			float lightLen = length(lightDir);
			lightDir *= 1.0 / (lightLen + Epsilon);

			directColor = BrdfGGX(diffuseColor, specularColor, gb.roughness, normalInWS, lightDir, -viewDirInWS)
				* plightColor
				* PointLightAttenuation(lightLen, lightPos.posAndRadius.w);
			finalColor += directColor;
		}
	}
#endif

	// RTXGI compute irradiance.
	float3 irradiance = 0;
	if (cbLight.giIntensity > 0.0)
	{
		DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumeDesc[0]);	// only one DDGIVolume.
		float3 surfaceBias = DDGIGetSurfaceBias(normalInWS, -viewDirInWS, volume);

		// setup ddgi resources.
		DDGIVolumeResources resources;
		resources.probeIrradiance = DDGIProbeIrradiance;
		resources.probeDistance = DDGIProbeDistance;
		resources.probeData = DDGIProbeData;
		resources.bilinearSampler = samHDRI;

		// get volume blend weight
		float volumeBlendWeight = DDGIGetVolumeBlendWeight(worldPos.xyz, volume);

		if (volumeBlendWeight > 0.0)
		{
			// get irradiance.
			irradiance = DDGIGetVolumeIrradiance(
				worldPos.xyz,
				surfaceBias,
				normalInWS,
				volume,
				resources);
			// attenuation.
			irradiance *= volumeBlendWeight;
		}

		if (cbScene.renderColorSpace != 0)
		{
			irradiance = Rec709ToRec2020(irradiance);
		}
		irradiance = (diffuseColor / PI) * irradiance * cbLight.giIntensity;
	}
	finalColor += irradiance;

	return finalColor;
}

float2 CubemapDirToPanoramaUV(float3 dir)
{
	return float2(
		0.5f + 0.5f * atan2(dir.z, dir.x) / PI,
		acos(dir.y) / PI);
}

float3 SampleHDRI(uint2 pixelPos)
{
	float2 screenPos = ((float2)pixelPos + 0.5) / cbScene.screenInfo.zw;
	float2 clipSpacePos = screenPos * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, 1, 1));
	worldPos.xyz /= worldPos.w;

	float3 viewDirInWS = normalize(worldPos.xyz - cbScene.camPos.xyz);
	float2 panoramaUV = CubemapDirToPanoramaUV(viewDirInWS);
	return texHDRI.SampleLevel(samHDRI, panoramaUV, 0).rgb;
}

[numthreads(8, 8, 1)]
void main(
	uint3 gid : SV_GroupID,
	uint3 gtid : SV_GroupThreadID,
	uint3 did : SV_DispatchThreadID)
{
	uint2 pixelPos = did.xy;

	if (all(pixelPos < (uint2)cbScene.screenInfo.zw))
	{
		float depth = texDepth[pixelPos];
		[branch]
		if (depth >= 1.0)
		{
			float3 hdri = SampleHDRI(pixelPos);
			if (cbScene.renderColorSpace != 0)
			{
				hdri = Rec709ToRec2020(hdri);
			}
			rwOutput[pixelPos] = float4(hdri, 1);
		}
		else
		{
			rwOutput[pixelPos] = float4(Lighting(pixelPos, depth), 1);
		}
	}
}