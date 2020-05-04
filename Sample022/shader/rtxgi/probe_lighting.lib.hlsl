#define RTXGI_DDGI_PROBE_RELOCATION			1
#define RTXGI_DDGI_PROBE_STATE_CLASSIFIER	1

#include "ddgi/Irradiance.hlsl"
#include "ddgi/../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "../common.hlsli"
#include "../payload.hlsli"
#include "../raytrace.hlsli"

#define TMax			10000.0

// global
RaytracingAccelerationStructure			Scene					: register(t0, space0);
Texture2D<float4>						DDGIProbeIrradianceSRV	: register(t1);
Texture2D<float4>						DDGIProbeDistanceSRV	: register(t2);
Texture2D								SkyIrrTex				: register(t3);

RWTexture2D<float4>						DDGIProbeRTRadiance		: register(u0);
RWTexture2D<float4>						DDGIProbeOffsets		: register(u1);
RWTexture2D<uint>						DDGIProbeStates			: register(u2);

ConstantBuffer<DDGIVolumeDescGPU>		cbDDGIVolume			: register(b0);
ConstantBuffer<LightCB>					cbLight					: register(b1);

SamplerState							TrilinearSampler		: register(s0);
SamplerState							SkyIrrTex_s				: register(s1);


[shader("raygeneration")]
void ProbeLightingRGS()
{
	float4 result = 0.f;

	uint2 DispatchIndex = DispatchRaysIndex().xy;
	int rayIndex = DispatchIndex.x;						// index of ray within a probe
	int probeIndex = DispatchIndex.y;					// index of current probe

	int2 texelPosition = DDGIGetProbeTexelPosition(probeIndex, cbDDGIVolume.probeGridCounts);
	int  probeState = DDGIProbeStates[texelPosition];
	if (probeState == PROBE_STATE_INACTIVE)
	{
		//return;		// if the probe is inactive, do not shoot rays
	}

	// get ray origin and direction.
	float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(probeIndex, cbDDGIVolume.origin, cbDDGIVolume.probeGridCounts, cbDDGIVolume.probeGridSpacing, DDGIProbeOffsets);
	float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, cbDDGIVolume.numRaysPerProbe, cbDDGIVolume.probeRayRotationTransform);

	// trace probe ray.
	RayDesc ray = { probeWorldPosition, 0.0, probeRayDirection, 1e27 };
	MaterialPayload payload = (MaterialPayload)0;
	payload.hitT = -1.0;
	TraceRay(Scene, RAY_FLAG_NONE, ~0, kMaterialContribution, kGeometricContributionMult, 0, ray, payload);

	// ray miss.
	if (payload.hitT < 0.0)
	{
		DDGIProbeRTRadiance[DispatchIndex.xy] = float4(0, 0, 0, 1e27);
		return;
	}

	MaterialParam mat_param;
	DecodeMaterialPayload(payload, mat_param);

	// ray hit back face.
	if (mat_param.flag & kFlagBackFaceHit)
	{
		DDGIProbeRTRadiance[DispatchIndex.xy] = float4(0.f, 0.f, 0.f, -mat_param.hitT * 0.2f);
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
		RayDesc shadow_ray = { shadow_orig, 0.0, shadow_dir, TMax };
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
		float3 surfaceBias = DDGIGetSurfaceBias(mat_param.normal, ray.Direction, cbDDGIVolume);

		DDGIVolumeResources resources;
		resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
		resources.probeDistanceSRV = DDGIProbeDistanceSRV;
		resources.trilinearSampler = TrilinearSampler;
		resources.probeOffsets = DDGIProbeOffsets;
		resources.probeStates = DDGIProbeStates;

		float3 hit_pos = probeWorldPosition + probeRayDirection * payload.hitT;
		float volumeBlendWeight = DDGIGetVolumeBlendWeight(hit_pos, cbDDGIVolume);

		// if inside volume, blend weight > 0.
		if (volumeBlendWeight > 0)
		{
			irradiance = DDGIGetVolumeIrradiance(
				hit_pos,
				surfaceBias,
				mat_param.normal,
				cbDDGIVolume,
				resources);

			irradiance *= volumeBlendWeight;
		}
	}

	// output.
	result = float4(diffuse + ((diffuseColor / PI) * irradiance), mat_param.hitT);
	DDGIProbeRTRadiance[DispatchIndex.xy] = result;
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
