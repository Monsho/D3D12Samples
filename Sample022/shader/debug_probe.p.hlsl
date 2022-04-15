#include "common.hlsli"
#include "rtxgi/rtxgi_common_defines.hlsli"
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

StructuredBuffer<DDGIVolumeDescGPUPacked>	DDGIVolumeDesc			: register(t0);
Texture2D<float4>							DDGIProbeIrradiance		: register(t1);
Texture2D<float4>							DDGIProbeDistance		: register(t2);
Texture2D<float4>							DDGIProbeData			: register(t3);

SamplerState		BilinearWrapSampler	: register(s0);

float4 main(PSInput In) : SV_TARGET0
{
	float3 V = normalize(cbScene.camPos.xyz - In.posInWS);

	// RTXGI compute irradiance.
	float3 irradiance = 0;
	{
		DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumeDesc[0]);	// only one DDGIVolume.
		float3 surfaceBias = DDGIGetSurfaceBias(In.normal, -V, volume);

		DDGIVolumeResources resources;
		resources.probeIrradiance = DDGIProbeIrradiance;
		resources.probeDistance = DDGIProbeDistance;
		resources.probeData = DDGIProbeData;
		resources.bilinearSampler = BilinearWrapSampler;

		irradiance = DDGIGetVolumeIrradiance(
			In.posInWS,
			surfaceBias,
			In.normal,
			volume,
			resources);
	}
	irradiance *= cbLight.giIntensity;

	return float4(pow(saturate(irradiance), 1 / 2.2), 1);
}
