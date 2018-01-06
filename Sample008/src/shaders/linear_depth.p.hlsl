#include "const_buffer.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

Texture2D	texDepth;

float main(VSOutput In) : SV_TARGET0
{
	uint2 frameUV = (uint2)(In.uv * screenInfo.xy);

	// テクスチャサンプリング
	float depth = texDepth[frameUV].r;

	// リニア深度を求める
	float n = screenInfo.z;
	float f = screenInfo.w;
	return n / (f - depth * (f - n));
}


//	EOF
