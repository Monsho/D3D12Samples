#include "common.hlsli"
#include "ddgi/Irradiance.hlsl"
#include "ddgi/../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	posInWS		: POS_IN_WS;
	float3	normal		: NORMAL;
};

ConstantBuffer<SceneCB>				cbScene			: register(b0);
ConstantBuffer<LightCB>				cbLight			: register(b1);
ConstantBuffer<DDGIVolumeDescGPU>	cbDDGIVolume	: register(b2);

Texture2D<float4>	DDGIProbeIrradianceSRV	: register(t0);
Texture2D<float4>	DDGIProbeDistanceSRV	: register(t1);

RWTexture2D<float4>	DDGIProbeOffsets	: register(u0);
RWTexture2D<uint>	DDGIProbeStates		: register(u1);

SamplerState		TrilinearSampler	: register(s0);

float4 main(PSInput In) : SV_TARGET0
{
	float3 V = normalize(cbScene.camPos.xyz - In.posInWS);

	// RTXGI compute irradiance.
	float3 irradiance = 0;
	{
		float3 surfaceBias = DDGIGetSurfaceBias(In.normal, -V, cbDDGIVolume);

		DDGIVolumeResources resources;
		resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
		resources.probeDistanceSRV = DDGIProbeDistanceSRV;
		resources.trilinearSampler = TrilinearSampler;
		resources.probeOffsets = DDGIProbeOffsets;
		resources.probeStates = DDGIProbeStates;

		irradiance = DDGIGetVolumeIrradiance(
			In.posInWS,
			surfaceBias,
			In.normal,
			cbDDGIVolume,
			resources);
	}
	irradiance *= cbLight.giIntensity;

	return float4(pow(saturate(irradiance), 1 / 2.2), 1);
}
