#include "common.hlsli"
#include "constant.h"
#include "reflection_ray.hlsli"

#define BINNING_TILE_SIZE		32
#define THREAD_SIZE				BINNING_TILE_SIZE * BINNING_TILE_SIZE

#ifndef WITHOUT_BINNING
#	define WITHOUT_BINNING		0
#endif


uint RayDirectionToBin(float3 dir, uint2 TileSize)
{
	float2 oct = UnitVectorToOctahedron(dir);
	uint2 tilePix = min((uint2)floor(oct * TileSize), TileSize - 1);
	return MortonEncode2D(tilePix);
}


ConstantBuffer<SceneCB>			cbScene			: register(b0);

Texture2D						texGBuffer0		: register(t0);
Texture2D						texGBuffer1		: register(t1);
Texture2D						texGBuffer2		: register(t2);
Texture2D<float>				texDepth		: register(t3);
Texture2D<float2>				texBlueNoise	: register(t4);

RWStructuredBuffer<RayData>		rwRayData		: register(u0);

groupshared uint BinCounts[THREAD_SIZE];

#define MIN_WAVE_SIZE		32
#define MAX_WAVE_SUMS		((THREAD_SIZE + MIN_WAVE_SIZE - 1) / MIN_WAVE_SIZE)
groupshared uint WaveSums[MAX_WAVE_SUMS];


[numthreads(THREAD_SIZE, 1, 1)]
void main(
	uint gtid : SV_GroupThreadID,
	uint gid : SV_GroupID)
{
	// reset bin counts.
	BinCounts[gtid] = 0;
	GroupMemoryBarrierWithGroupSync();

	const uint2 TileSize = uint2(BINNING_TILE_SIZE, BINNING_TILE_SIZE);
	const uint TileIndex = gid;
	const uint RayIndex = gtid;
	const uint TileCountXY = (uint2(cbScene.screenInfo.zw) + TileSize - 1) / TileSize;
	const uint2 TilePos = uint2(TileIndex % TileCountXY.x, TileIndex / TileCountXY.x);
	const uint2 PixelPos = TilePos * TileSize + uint2(RayIndex % TileSize.x, RayIndex / TileSize.y);

	RayData Ray = (RayData)0;
	uint Bin = THREAD_SIZE - 1;
	float depth = texDepth[PixelPos];

	Ray.packedPixelPos = PixelPos.x | (PixelPos.y << 16);
	if (any(PixelPos >= uint2(cbScene.screenInfo.zw)) || depth >= 1.0)
	{
		Ray.flag = 0x0;
	}
	else
	{
		// get gbuffer.
		float4 gin0 = texGBuffer0[PixelPos];
		float4 gin1 = texGBuffer1[PixelPos];
		float4 gin2 = texGBuffer2[PixelPos];
		GBuffer gb = DecodeGBuffer(gin0, gin1, gin2);

		// get world position.
		float2 screenPos = ((float2)PixelPos + 0.5) / cbScene.screenInfo.zw;
		float2 clipSpacePos = screenPos * float2(2, -2) + float2(-1, 1);
		float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
		worldPos.xyz /= worldPos.w;

		// get mirror reflection ray.
		// TODO: implement rough surface reflection ray.
		float3 normalInWS = ConvertVectorTangentToWorld(float3(0, 0, 1), gb.worldQuat);
		float3 viewDir = normalize(worldPos.xyz - cbScene.camPos.xyz);
#if 0
		float3 reflectDir = reflect(viewDir, normalInWS);
#else
		float2 noiseU = frac(texBlueNoise[PixelPos % 128] + (cbScene.frameIndex & 0xff) * kGoldenRatio);
		float3 reflectDir = SampleReflectionRay(viewDir, gb.worldQuat, gb.roughness, noiseU);
#endif
		Ray.origin = worldPos + normalInWS * 0.01;
		Ray.direction = reflectDir;
		Ray.flag = 0x1;

		Bin = RayDirectionToBin(Ray.direction, TileSize);
	}

#if WITHOUT_BINNING		// no binning.
	uint StoreIndex = TileIndex * (TileSize.x * TileSize.y) + RayIndex;

	rwRayData[StoreIndex] = Ray;

#else
	// store bin.
	uint ThisThreadBinCount = 0;
	InterlockedAdd(BinCounts[Bin], 1, ThisThreadBinCount);
	GroupMemoryBarrierWithGroupSync();

	// binning.
#if 1		// enable wave ops.
	const uint WaveIndex = gtid / WaveGetLaneCount();
	const uint LaneIndex = WaveGetLaneIndex();
	uint Value = BinCounts[gtid];
	uint ThisWaveSum = WaveActiveSum(Value);
	if (LaneIndex == 0)
	{
		WaveSums[WaveIndex] = ThisWaveSum;
	}
	GroupMemoryBarrierWithGroupSync();
	if (WaveIndex == 0 && LaneIndex < MAX_WAVE_SUMS)
	{
		WaveSums[LaneIndex] = WavePrefixSum(WaveSums[LaneIndex]);
	}
	GroupMemoryBarrierWithGroupSync();
	BinCounts[gtid] = WaveSums[WaveIndex] + WavePrefixSum(Value);
#else
	if (gtid == 0)
	{
		uint Counter = 0;
		for (uint i = 0; i < THREAD_SIZE; i++)
		{
			uint NextCounter = Counter + BinCounts[i];
			BinCounts[i] = Counter;
			Counter = NextCounter;
		}
	}
#endif

	GroupMemoryBarrierWithGroupSync();

	uint StoreIndex = TileIndex * (TileSize.x * TileSize.y) + BinCounts[Bin] + ThisThreadBinCount;

	rwRayData[StoreIndex] = Ray;
#endif
}

//	EOF
