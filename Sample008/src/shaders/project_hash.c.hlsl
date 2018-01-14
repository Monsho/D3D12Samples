// ハッシュのプロジェクション

#include "const_buffer.hlsli"

#define kTileWidth				(16)
#define kTileSize				(kTileWidth * kTileWidth)

// 定数バッファ
cbuffer CbWaterInfo
{
	float	waterHeight;
};


// 入力
Texture2D				texLinearDepth;

// 出力
RWTexture2D<uint>		rwProjectHash;

// ワールド座標を取得する
float3 GetSurfaceWorldPos(uint2 uv)
{
	float linearDepth = texLinearDepth[uv].r;

	float2 screen_uv = (float2)uv / screenInfo.xy;
	screen_uv = screen_uv * float2(2, -2) + float2(-1, 1);
	float3 frustumVec = { frustumCorner.x * screen_uv.x, frustumCorner.y * screen_uv.y, -frustumCorner.z };
	float3 posVS = frustumVec * linearDepth;
	return mul(mtxViewToWorld, float4(posVS, 1)).xyz;
}


[numthreads(kTileWidth, kTileWidth, 1)]
void main(
	uint3 groupId          : SV_GroupID,
	uint3 dispatchThreadId : SV_DispatchThreadID,
	uint3 groupThreadId : SV_GroupThreadID)
{
	// 各ピクセルの法線、深度、アルベドを取得する
	uint2 frameUV = dispatchThreadId.xy;
	float3 posWS = GetSurfaceWorldPos(frameUV);

	if (waterHeight >= posWS.y)
	{
		return;
	}

	// 水面に反射した座標を求める
	float3 posReflWS = float3(posWS.x, 2.0 * waterHeight - posWS.y, posWS.z);
	float4 posReflVS = mul(mtxWorldToView, float4(posReflWS, 1));
	float4 posReflCS = mul(mtxViewToClip, posReflVS);
	float2 posReflUV = posReflCS.xy / posReflCS.w * float2(0.5, -0.5) + 0.5;

	if (any(posReflUV < -1.0) || any(posReflUV > 1.0))
	{
		return;
	}

	// 反射先のUVに反射元のUVを格納する
	uint2 reflFrameUV = (uint2)(posReflUV * screenInfo.xy);
	uint phash = frameUV.y << 16 | frameUV.x;
	InterlockedMax(rwProjectHash[reflFrameUV], phash);
}

//	EOF
