#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);

Texture2D<float2>			texDepth		: register(t0);

float2 main(PSInput In) : SV_Target0
{
	uint2 uv = (uint2)In.position.xy;
	float2 d0 = texDepth[uv * 2 + uint2(0, 0)];
	float2 d1 = texDepth[uv * 2 + uint2(1, 0)];
	float2 d2 = texDepth[uv * 2 + uint2(0, 1)];
	float2 d3 = texDepth[uv * 2 + uint2(1, 1)];
	float minD = min(d0.x, min(d1.x, min(d2.x, d3.x)));
	float maxD = max(d0.y, max(d1.y, max(d2.y, d3.y)));
	return float2(minD, maxD);
}
