#include "common.hlsli"
#include "culling.hlsli"

RWByteAddressBuffer			rwArgCounter		: register(u0);
RWByteAddressBuffer			rwNegCounter		: register(u1);
RWByteAddressBuffer			rwDrawCounter		: register(u2);

[numthreads(32, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint submesh_index = dispatchID.x;
	rwArgCounter.Store(4 * submesh_index * 2, 0);
	rwArgCounter.Store(4 * (submesh_index * 2 + 1), 0);
	rwNegCounter.Store(4 * submesh_index, 0);
	rwDrawCounter.Store(0, 0);
}
