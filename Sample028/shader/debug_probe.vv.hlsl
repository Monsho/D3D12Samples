#include "rtxgi.hlsli"
#include "common.hlsli"

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

StructuredBuffer<DDGIVolumeDescGPUPacked>	DDGIVolumeDesc			: register(t0);
Texture2D<float4>							DDGIProbeData			: register(t1);

VSOutput main(const VSInput In, uint instanceIndex : SV_InstanceID)
{
	const float kSphereRadius = 0.2f;

	VSOutput Out;

	// get volume desc.
	DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumeDesc[0]);

	// get probe world position.
	float3 probeCoords = DDGIGetProbeCoords(instanceIndex, volume);
	float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, DDGIProbeData);

	float3 wp = In.position * kSphereRadius + probeWorldPosition;
	Out.posInWS = wp;
	Out.position = mul(cbScene.mtxWorldToProj, float4(Out.posInWS, 1));
	Out.normal = In.position;

	return Out;
}

//	EOF
