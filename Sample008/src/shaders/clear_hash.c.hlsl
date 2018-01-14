// ハッシュバッファのクリア

#include "const_buffer.hlsli"

#define kTileWidth				(16)
#define kTileSize				(kTileWidth * kTileWidth)

// 出力
RWTexture2D<uint>		rwProjectHash;

[numthreads(kTileWidth, kTileWidth, 1)]
void main(
	uint3 groupId          : SV_GroupID,
	uint3 dispatchThreadId : SV_DispatchThreadID,
	uint3 groupThreadId : SV_GroupThreadID)
{
	uint2 frameUV = dispatchThreadId.xy;
	rwProjectHash[frameUV] = 0;
}

//	EOF
