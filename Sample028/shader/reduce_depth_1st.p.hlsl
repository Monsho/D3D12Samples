#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
};

ConstantBuffer<SceneCB>					cbScene			: register(b0);

Texture2D<float>	texDepth		: register(t0);

float2 main(PSInput In) : SV_Target0
{
	uint2 uv = (uint2)In.position.xy;
	float d0 = texDepth[uv * 2 + uint2(0, 0)];
	float d1 = texDepth[uv * 2 + uint2(1, 0)];
	float d2 = texDepth[uv * 2 + uint2(0, 1)];
	float d3 = texDepth[uv * 2 + uint2(1, 1)];

	float minD = min(d0, min(d1, min(d2, d3)));
	float maxD = max(d0, max(d1, max(d2, d3)));
	return float2(minD, maxD);
}
