#include "common.hlsli"

ConstantBuffer<SceneCB>			cbScene			: register(b0);
ConstantBuffer<TileCB>			cbTile			: register(b1);
ConstantBuffer<VariableRateCB>	cbVariableRate	: register(b2);

Texture2D			texNormalWS			: register(t0);
Texture2D<float>	texDepth			: register(t1);

RWTexture2D<uint>	rwVRSImage			: register(u0);

float GetLinearDepth(float depth, float nz, float fz)
{
	return nz * fz / (depth * (nz - fz) + fz);
}

[numthreads(8, 4, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	// VRSイメージ外の処理は行わない
	uint2 index = dispatchID.xy;
	if (index.x >= cbTile.imageWidth || index.y >= cbTile.imageHeight)
	{
		return;
	}

	uint tile_count = cbTile.tileSize / 2;
	uint2 uv = index * cbTile.tileSize;
	uint min_vrs = cbVariableRate.vrsType;
	uint loop_count = tile_count * tile_count;
	for (uint i = 0; i < loop_count; i++)
	{
		// 深度差をチェック
		uint2 xy = uint2((i % tile_count) * 2, (i / tile_count) * 2) + uv;
		float depth[4] = {
			//texDepth[xy],
			//texDepth[xy + uint2(1, 0)],
			//texDepth[xy + uint2(0, 1)],
			//texDepth[xy + uint2(1, 1)]
			GetLinearDepth(texDepth[xy], cbScene.screenInfo.x, cbScene.screenInfo.y),
			GetLinearDepth(texDepth[xy + uint2(1, 0)], cbScene.screenInfo.x, cbScene.screenInfo.y),
			GetLinearDepth(texDepth[xy + uint2(0, 1)], cbScene.screenInfo.x, cbScene.screenInfo.y),
			GetLinearDepth(texDepth[xy + uint2(1, 1)], cbScene.screenInfo.x, cbScene.screenInfo.y)
		};

		float depth_d = max(max(abs(depth[0] - depth[1]), abs(depth[2] - depth[3])), max(abs(depth[0] - depth[2]), abs(depth[1] - depth[3])));
		if (depth_d > cbVariableRate.depthThreashold)
		{
			min_vrs = 0x0;
			break;
		}

		// ノーマル差をチェック
		float3 normalWS[4] = {
			texNormalWS[xy].xyz * 2 - 1,
			texNormalWS[xy + uint2(1, 0)].xyz * 2 - 1,
			texNormalWS[xy + uint2(0, 1)].xyz * 2 - 1,
			texNormalWS[xy + uint2(1, 1)].xyz * 2 - 1,
		};
		float normal_d = min(min(dot(normalWS[0], normalWS[1]), dot(normalWS[2], normalWS[3])), min(dot(normalWS[0], normalWS[2]), dot(normalWS[1], normalWS[3])));
		if (normal_d < cbVariableRate.normalThreashold)
		{
			min_vrs = 0x0;
			break;
		}
	}

	rwVRSImage[index] = min_vrs;
}