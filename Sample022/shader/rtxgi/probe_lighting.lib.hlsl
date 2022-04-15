#define RTXGI_DDGI_PROBE_RELOCATION			1
#define RTXGI_DDGI_PROBE_STATE_CLASSIFIER	1

#include "rtxgi_common_defines.hlsli"
#include "ddgi/Irradiance.hlsl"
#include "ddgi/../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "../common.hlsli"
#include "../payload.hlsli"
#include "../raytrace.hlsli"

#define kShadowRayTMax			10000.0

// global
RaytracingAccelerationStructure				Scene					: register(t0, space0);
StructuredBuffer<DDGIVolumeDescGPUPacked>	DDGIVolumeDesc			: register(t1);
Texture2D<float4>							DDGIProbeIrradiance		: register(t2);
Texture2D<float4>							DDGIProbeDistance		: register(t3);
Texture2D<float4>							DDGIProbeData			: register(t4);
Texture2D									SkyIrrTex				: register(t5);

RWTexture2D<float4>							DDGIRayData				: register(u0);

ConstantBuffer<DDGIConstants>				cbDDGIConst				: register(b0);
ConstantBuffer<LightCB>						cbLight					: register(b1);

SamplerState								BilinearWrapSampler		: register(s0);
SamplerState								SkyIrrTex_s				: register(s1);


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
	TraceRay(Scene, RAY_FLAG_NONE, ~0, kMaterialContribution, kGeometricContributionMult, 0, ray, payload);

	// ray miss.
	if (payload.hitT < 0.0)
	{
		//float3 SkyLight = float3(1,0,0);
		float3 SkyLight = SkyTextureLookup(SkyIrrTex, SkyIrrTex_s, probeRayDirection) * cbLight.skyPower;
		DDGIStoreProbeRayMiss(DDGIRayData, texCoords, volume, SkyLight);
		return;
	}

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

	float3 diffuse = 0;
	float3 diffuseColor = mat_param.baseColor.rgb * (1 - mat_param.metallic);
	float3 shadow_pos = probeWorldPosition + probeRayDirection * payload.hitT;
	float3 shadow_orig = shadow_pos + mat_param.normal * 1e-2;
	// compute diffuse from directional light.
	{
		// prepare shadow ray.
		float3 shadow_dir = -cbLight.lightDir.xyz;

		// shadow ray trace.
		RayDesc shadow_ray;
		shadow_ray.Origin = shadow_orig;
		shadow_ray.Direction = shadow_dir;
		shadow_ray.TMin = 0.0;
		shadow_ray.TMax = kShadowRayTMax;
		HitPayload shadow_pay;
		shadow_pay.hitT = -1;
		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, kShadowContribution, kGeometricContributionMult, 1, shadow_ray, shadow_pay);
		float shadow_factor = (shadow_pay.hitT >= 0.0) ? 0.0 : 1.0;

		// lighting.
		float3 N = mat_param.normal;
		float3 L = -cbLight.lightDir.xyz;
		float NoL = saturate(dot(N, L));
		float3 DResult = DiffuseLambert(diffuseColor);
		float3 skyColor = SkyTextureLookup(SkyIrrTex, SkyIrrTex_s, N) * cbLight.skyPower * diffuseColor;
		diffuse = DResult * NoL * cbLight.lightColor.rgb * shadow_factor + skyColor;
	}
	// compute diffuse from spot light.
	{
		float3 spotPos = cbLight.spotLightPosAndRadius.xyz;
		float spotR = cbLight.spotLightPosAndRadius.w;
		float3 spotDir = cbLight.spotLightDirAndCos.xyz;
		float spotCos = cbLight.spotLightDirAndCos.w;
		float3 direction = shadow_orig - spotPos;
		float len = length(direction);
		direction *= 1.0 / len;
		float attn = ComputeSpotLightAttenuation(spotDir, spotR, spotCos, direction, len);

		//[branch]
		if (attn > 0)
		{
			// prepare shadow ray.
			RayDesc shadow_ray = { shadow_orig, 0.0, -direction, len };
			HitPayload shadow_pay;
			shadow_pay.hitT = -1;

			// trace ray.
			TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, kShadowContribution, kGeometricContributionMult, 1, shadow_ray, shadow_pay);
			float shadow_factor = (shadow_pay.hitT >= 0.0) ? 0.0 : 1.0;

			// lighting.
			float3 N = mat_param.normal;
			float3 L = -direction;
			float NoL = saturate(dot(N, L));
			float3 DResult = DiffuseLambert(diffuseColor);
			diffuse += DResult * NoL * cbLight.spotLightColor.rgb * shadow_factor * attn;
		}
	}

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
		float volumeBlendWeight = DDGIGetVolumeBlendWeight(shadow_pos, volume);

		if (volumeBlendWeight > 0.0)
		{
			// get irradiance.
			irradiance = DDGIGetVolumeIrradiance(
				shadow_pos,
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
void MaterialMS(inout MaterialPayload payload : SV_RayPayload)
{
	// not implement.
}

[shader("miss")]
void DirectShadowMS(inout HitPayload payload : SV_RayPayload)
{
	// not implement.
}

// EOF
