#include "common.hlsli"
#include "ddgi/Irradiance.hlsl"
#include "ddgi/../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"

struct VSInput
{
	float3	position	: POSITION;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	posInWS		: POS_IN_WS;
	float3	normal		: NORMAL;
};

ConstantBuffer<SceneCB>				cbScene			: register(b0);
ConstantBuffer<DDGIVolumeDescGPU>	cbDDGIVolume	: register(b1);

//RWTexture2D<float4>	DDGIProbeOffsets	: register(u0);

VSOutput main(const VSInput In, uint instanceIndex : SV_InstanceID)
{
	const float kSphereRadius = 0.2f;

	VSOutput Out;

	float3 probeWorldPosition = DDGIGetProbeWorldPosition(instanceIndex, cbDDGIVolume.origin, cbDDGIVolume.probeGridCounts, cbDDGIVolume.probeGridSpacing);
	//float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(instanceIndex, cbDDGIVolume.origin, cbDDGIVolume.probeGridCounts, cbDDGIVolume.probeGridSpacing, DDGIProbeOffsets);

	float3 wp = In.position * kSphereRadius + probeWorldPosition;
	Out.posInWS = wp;
	Out.position = mul(cbScene.mtxWorldToProj, float4(Out.posInWS, 1));
	Out.normal = In.position;

	return Out;
}

//	EOF
