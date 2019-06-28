#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);

Texture2D			texNormal		: register(t0);
Texture2D<float>	texDepth		: register(t1);
Texture2D			texResult		: register(t2);
Texture2D			texPrevNormal	: register(t3);
Texture2D<float>	texPrevDepth	: register(t4);
Texture2D			texPrevResult	: register(t5);
SamplerState		samLinearClamp	: register(s0);
SamplerState		samPointClamp	: register(s1);

float4 main(PSInput In) : SV_TARGET0
{
	float3 normal = normalize(texNormal[In.position.xy].xyz * 2 - 1);
	float depth = texDepth[In.position.xy];
	float4 result = texResult[In.position.xy];

	// ワールド空間座標を計算
	float2 posCS = In.uv * float2(2, -2) + float2(-1, 1);
	float4 posWS = mul(cbScene.mtxProjToWorld, float4(posCS, depth, 1));
	posWS.xyz /= posWS.w;

	// 前回のスクリーン空間に変換
	float4 prevPosCS = mul(cbScene.mtxPrevWorldToProj, float4(posWS.xyz, 1));
	prevPosCS.xyz /= prevPosCS.w;
	float2 prevUV = prevPosCS.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

	// 前回の情報を確保
	float3 prevNormal = normalize(texPrevNormal.SampleLevel(samLinearClamp, prevUV, 0).xyz * 2 - 1);
	float prevDepth = texPrevDepth.SampleLevel(samLinearClamp, prevUV, 0);
	float4 prevRes = texPrevResult.SampleLevel(samPointClamp, prevUV, 0);

	// ウェイト値
	float n = cbScene.screenInfo.x;
	float f = cbScene.screenInfo.y;
	float nw = dot(normal, prevNormal) < 0.8 ? 0 : 1;
	float nd = abs(ToLinearDepth(depth, n, f) - ToLinearDepth(prevDepth, n, f)) > 0.0001 ? 0 : 1;
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
