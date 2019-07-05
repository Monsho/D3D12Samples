#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);

Texture2D			texNormal		: register(t0);
Texture2D			texDepth		: register(t1);
Texture2D			texMotion		: register(t2);
Texture2D			texResult		: register(t3);
Texture2D			texPrevNormal	: register(t4);
Texture2D			texPrevDepth	: register(t5);
Texture2D			texPrevResult	: register(t6);
SamplerState		samLinearClamp	: register(s0);
SamplerState		samPointClamp	: register(s1);

float4 main(PSInput In) : SV_TARGET0
{
	float3 normal = normalize(texNormal[In.position.xy].xyz * 2 - 1);
	float4 depth = texDepth[In.position.xy];
	float4 result = texResult[In.position.xy];

	[branch]
	if (!cbScene.temporalOn)
	{
		return result;
	}

	float4 motion = texMotion[In.position.xy];
	float2 prevUV = In.uv - motion.xy;

	// 前回の情報を確保
	float3 prevNormal = normalize(texPrevNormal.SampleLevel(samPointClamp, prevUV, 0).xyz * 2 - 1);
	float4 prevDepth = texPrevDepth.SampleLevel(samPointClamp, prevUV, 0);
	float4 prevRes = texPrevResult.SampleLevel(samPointClamp, prevUV, 0);

	// ウェイト値
	//float nw = 1;
	float nw = distance(normal, prevNormal) / (depth.w + 1e-2) > 16.0 ? 0 : 1;
	float nd = abs(prevDepth.x - depth.x) / (depth.y + 1e-4) > 2.0 ? 0 : 1;
	float weight = nw * nd;
	weight *= any(prevUV < 0) || any(prevUV > 1) ? 0 : 1;

	if (weight > 0.0)
	{
		float n = prevRes.z + 1;
		result.y = (prevRes.y * prevRes.z + result.y) / n;
		result.z = min(n, 512);
	}
	else
	{
		result.z = 1;
	}

	return result;
}
