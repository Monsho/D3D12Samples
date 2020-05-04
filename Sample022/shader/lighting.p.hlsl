#include "common.hlsli"
#include "ddgi/Irradiance.hlsl"
#include "ddgi/../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD0;
};

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);
ConstantBuffer<ReflectionCB>			cbReflection	: register(b2);
ConstantBuffer<DDGIVolumeDescGPU>		cbDDGIVolume	: register(b3);

Texture2D			texNormal			: register(t0);
Texture2D			texBaseColor		: register(t1);
Texture2D			texMotionRM			: register(t2);
Texture2D<float>	texDepth			: register(t3);
Texture2D<float2>	texScreenShadow		: register(t4);
Texture2D			texReflection		: register(t5);
Texture2D			texSkyHdr			: register(t6);
Texture2D<float4>	DDGIProbeIrradianceSRV	: register(t7);
Texture2D<float4>	DDGIProbeDistanceSRV	: register(t8);

RWTexture2D<float4>	DDGIProbeOffsets	: register(u0);
RWTexture2D<uint>	DDGIProbeStates		: register(u1);

SamplerState		texColor_s			: register(s0);
SamplerState		texSkyHdr_s			: register(s1);
SamplerState		TrilinearSampler	: register(s2);

float4 main(PSInput In) : SV_TARGET0
{
	// from depth to world position.
	uint2 pixel = uint2(In.position.xy);
	float depth = texDepth[pixel];
	if (depth >= 1.0)
	{
		return float4(0, 0, 1, 1);
	}
	float2 clipSpacePos = In.uv * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	float4 baseColor = texBaseColor[pixel];
	float3 normalInWS = texNormal[pixel].xyz * 2.0 - 1.0;
	float2 rm = texMotionRM[pixel].zw;
	float2 direct_shadow = texScreenShadow[pixel];

	float roughness = rm.x + Epsilon;
	float metallic = rm.y;

	float3 diffuseColor = baseColor.rgb * (1 - metallic);
	float3 specularColor = 0.04 * (1 - metallic) + baseColor.rgb * metallic;

	float3 V = normalize(cbScene.camPos.xyz - worldPos.xyz);

	// directional light.
	float3 directionalColor = 0;
	{
		directionalColor = BrdfGGX(diffuseColor, specularColor, roughness, normalInWS, -cbLight.lightDir.rgb, V) * cbLight.lightColor.rgb;
	}

	// spot light.
	float3 spotColor = 0;
	{
		float3 dir = worldPos.xyz - cbLight.spotLightPosAndRadius.xyz;
		float len = length(dir);
		dir *= 1.0 / len;
		float attn = ComputeSpotLightAttenuation(
			cbLight.spotLightDirAndCos.xyz, cbLight.spotLightPosAndRadius.w, cbLight.spotLightDirAndCos.w,
			dir, len);
		[branch]
		if (attn > 0)
		{
			spotColor = BrdfGGX(diffuseColor, specularColor, roughness, normalInWS, -dir, V) * cbLight.spotLightColor.rgb * attn;
		}
	}

	// reflection.
	float3 reflectionColor = SpecularFSchlick(saturate(dot(V, normalInWS)), baseColor.rgb * metallic)
		* texReflection.SampleLevel(texColor_s, In.uv, 0).rgb * cbReflection.intensity;

	// sky light.
	float3 skyColor = SkyTextureLookup(texSkyHdr, texSkyHdr_s, normalInWS) * cbLight.skyPower * diffuseColor;

	// RTXGI compute irradiance.
	float3 irradiance = 0;
	{
		float3 surfaceBias = DDGIGetSurfaceBias(normalInWS, -V, cbDDGIVolume);

		DDGIVolumeResources resources;
		resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
		resources.probeDistanceSRV = DDGIProbeDistanceSRV;
		resources.trilinearSampler = TrilinearSampler;
		resources.probeOffsets = DDGIProbeOffsets;
		resources.probeStates = DDGIProbeStates;

		irradiance = DDGIGetVolumeIrradiance(
			worldPos.xyz,
			surfaceBias,
			normalInWS,
			cbDDGIVolume,
			resources);

		// todo: multiply ao.
	}

	float3 finalColor =
		directionalColor * direct_shadow.r
		+ spotColor * direct_shadow.g
		+ skyColor
		+ reflectionColor
		+ (diffuseColor / PI) * irradiance * cbLight.giIntensity;
	return float4(pow(saturate(finalColor), 1 / 2.2), 1);
}
