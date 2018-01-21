#include "const_buffer.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

Texture2D				texSceneColor;
Texture2D<uint>			texProjectHash;

float4 HashToColor(uint hash)
{
	uint2 colorUV = { hash & 0xffff, hash >> 16 };
	float4 ret = texSceneColor[colorUV];
	ret.w = 1.0;
	return ret;
}

float4 main(VSOutput In) : SV_TARGET0
{
	uint2 frameUV = (uint2)(In.uv * screenInfo.xy);

	// ハッシュを取得
	uint phash = texProjectHash[frameUV].x;

	// カラーを求める
	float4 ret = (float4)0;
	if (phash != 0)
	{
		ret = HashToColor(phash);
	}
	else if (gapBleed)
	{
		uint4 hash4 = {
			texProjectHash[frameUV + uint2(1, 0)].x,
			texProjectHash[frameUV + uint2(0, 1)].x,
			texProjectHash[frameUV - uint2(1, 0)].x,
			texProjectHash[frameUV - uint2(0, 1)].x,
		};
		float4 r0 = HashToColor(hash4.x);
		float4 r1 = HashToColor(hash4.y);
		float4 r2 = HashToColor(hash4.z);
		float4 r3 = HashToColor(hash4.w);
		ret += r0 * r0.a;
		ret += r1 * r1.a;
		ret += r2 * r2.a;
		ret += r3 * r3.a;
		ret /= (r0.a + r1.a + r2.a + r3.a);
	}

	return ret;
}


//	EOF
