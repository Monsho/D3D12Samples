#include "const_buffer.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

Texture2D				texSceneColor;
Texture2D<uint>			texProjectHash;

float4 main(VSOutput In) : SV_TARGET0
{
	uint2 frameUV = (uint2)(In.uv * screenInfo.xy);

	// ハッシュを取得
	uint phash = texProjectHash[frameUV].x;

	// カラーを求める
	float4 ret = (float4)0;
	if (phash != 0)
	{
		uint2 colorUV = { phash & 0xffff, phash >> 16 };
		ret = texSceneColor[colorUV];
		ret.w = 1.0;
	}

	return ret;
}


//	EOF
