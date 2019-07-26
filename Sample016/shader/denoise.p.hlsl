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

#define kWidth		(2)

float CalcWeight(int2 uv, int2 offset, float3 normal, float depth)
{
	float zn = cbScene.screenInfo.x;
	float zf = cbScene.screenInfo.y;
	float3 n = normalize(texNormal[uv + offset].xyz * 2 - 1);
	float d = texDepth[uv + offset];

	float w = 1.0;
	//float w = saturate(smoothstep(13, 0, dot(offset, offset)));
	w *= saturate(dot(normal, n));
	w *= saturate(smoothstep(0.0001, 0, abs(depth - ToLinearDepth(d, zn, zf))));
	return w;
}

float4 main(PSInput In) : SV_TARGET0
{
	float zn = cbScene.screenInfo.x;
	float zf = cbScene.screenInfo.y;

	int2 uv = int2(In.position.xy);
	float3 normal = normalize(texNormal[uv].xyz * 2 - 1);
	float depth = ToLinearDepth(texDepth[uv], zn, zf);
	float4 res = texResult[uv];
	float weight = 1.0;

	for (int x = -kWidth; x <= kWidth; x++)
	{
		for (int y = -kWidth; y <= kWidth; y++)
		{
			if (x == 0 && y == 0) continue;

			float w = CalcWeight(uv, int2(x, y), normal, depth);
			res.xy += texResult[uv + int2(x, y)].xy * w;
			weight += w;
		}
	}

	return float4(res.xy / weight, res.zw);
}
