#include "common.hlsli"

#ifndef CULLING_PHASE
#	define CULLING_PHASE 1
#endif // #ifndef CULLING_PHASE

ConstantBuffer<SceneCB>		cbScene			: register(b0);

#if CULLING_PHASE == 1
Texture2D<float>	texDepth		: register(t0);
#else
Texture2D<float2>	texDepth		: register(t0);
#endif

RWTexture2D<float2>	rwReduce0		: register(u0);
RWTexture2D<float2>	rwReduce1		: register(u1);
RWTexture2D<float2>	rwReduce2		: register(u2);

#define USE_MORTON_ORDER 1

[numthreads(32, 1, 1)]
void main(
	uint3 gid : SV_GroupID,
	uint3 gtid : SV_GroupThreadID,
	uint3 did : SV_DispatchThreadID)
{
	uint2 LeftTop = gid.xy * uint2(8, 4);
#if USE_MORTON_ORDER
	uint2 LocalPos = MortonDecode2D(gtid.x);
#else
	uint2 LocalPos = uint2(gtid.x % 8, gtid.x / 8);
#endif
	uint2 PixelPos = LeftTop + LocalPos;

	uint Width = 0, Height = 0;
	rwReduce0.GetDimensions(Width, Height);

#if CULLING_PHASE == 1
	float4 depth4 = float4(
		texDepth[PixelPos * 2 + uint2(0, 0)],
		texDepth[PixelPos * 2 + uint2(1, 0)],
		texDepth[PixelPos * 2 + uint2(0, 1)],
		texDepth[PixelPos * 2 + uint2(1, 1)]);
	float2 depth = float2(
		min(depth4.x, min(depth4.y, min(depth4.z, depth4.w))),
		max(depth4.x, max(depth4.y, max(depth4.z, depth4.w))));
#else
	float2 depth00 = texDepth[PixelPos * 2 + uint2(0, 0)];
	float2 depth10 = texDepth[PixelPos * 2 + uint2(1, 0)];
	float2 depth01 = texDepth[PixelPos * 2 + uint2(0, 1)];
	float2 depth11 = texDepth[PixelPos * 2 + uint2(1, 1)];
	float2 depth = float2(
		min(depth00.x, min(depth10.x, min(depth01.x, depth11.x))),
		max(depth00.y, max(depth10.y, max(depth01.y, depth11.y))));
#endif
	if (PixelPos.x < Width && PixelPos.y < Height)
	{
		rwReduce0[PixelPos] = depth;
	}

#if USE_MORTON_ORDER
	float2 d1 = WaveReadLaneAt(depth, gtid.x + 1);
	float2 d2 = WaveReadLaneAt(depth, gtid.x + 2);
	float2 d3 = WaveReadLaneAt(depth, gtid.x + 3);
#else
	float2 d1 = WaveReadLaneAt(depth, gtid.x + 1);
	float2 d2 = WaveReadLaneAt(depth, gtid.x + 8);
	float2 d3 = WaveReadLaneAt(depth, gtid.x + 9);
#endif
	depth = float2(
		min(depth.x, min(d1.x, min(d2.x, d3.x))),
		max(depth.y, max(d1.y, max(d2.y, d3.y))));
	PixelPos /= 2;
	Width /= 2;
	Height /= 2;
#if USE_MORTON_ORDER
	if (!(gtid.x & 0x3) && PixelPos.x < Width && PixelPos.y < Height)
#else
	if (!(LocalPos.x & 0x01) && !(LocalPos.y & 0x01) && PixelPos.x < Width && PixelPos.y < Height)
#endif
	{
		rwReduce1[PixelPos] = depth;
	}

#if USE_MORTON_ORDER
	d1 = WaveReadLaneAt(depth, gtid.x + 4);
	d2 = WaveReadLaneAt(depth, gtid.x + 8);
	d3 = WaveReadLaneAt(depth, gtid.x + 12);
#else
	d1 = WaveReadLaneAt(depth, gtid.x + 2);
	d2 = WaveReadLaneAt(depth, gtid.x + 16);
	d3 = WaveReadLaneAt(depth, gtid.x + 18);
#endif
	depth = float2(
		min(depth.x, min(d1.x, min(d2.x, d3.x))),
		max(depth.y, max(d1.y, max(d2.y, d3.y))));
	PixelPos /= 2;
	Width /= 2;
	Height /= 2;
#if USE_MORTON_ORDER
	if (!(gtid.x & 0xf) && PixelPos.x < Width && PixelPos.y < Height)
#else
	if (!(LocalPos.x & 0x3) && PixelPos.x < Width && PixelPos.y < Height)
#endif
	{
		rwReduce2[PixelPos] = depth;
	}
}
