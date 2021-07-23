#include "common.hlsli"
#include "culling.hlsli"

struct DrawArg
{
	uint		IndexCountPerInstance;
	uint		InstanceCount;
	uint		StartIndexLocation;
	int			BaseVertexLocation;
	uint		StartInstanceLocation;
};

ConstantBuffer<SceneCB>		cbScene				: register(b0);
ConstantBuffer<FrustumCB>	cbFrustum			: register(b1);
ConstantBuffer<MeshCB>		cbMesh				: register(b2);
ConstantBuffer<MeshletCB>	cbMeshlet			: register(b3);

StructuredBuffer<MeshletBound>		MeshletBounds		: register(t0);
StructuredBuffer<MeshletDrawInfo>	MeshletDrawInfos	: register(t1);
Texture2D<float2>					HiZ					: register(t2);

RWStructuredBuffer<DrawArg>	rwDrawArgs			: register(u0);
RWByteAddressBuffer			rwCounter			: register(u1);
RWStructuredBuffer<uint>	rwFalseNegative		: register(u2);
RWByteAddressBuffer			rwFNCounter			: register(u3);
RWByteAddressBuffer			rwDrawCounter		: register(u4);

[numthreads(32, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint fn_count = rwFNCounter.Load(cbMeshlet.falseNegativeCountByteOffset);
	uint fn_index = dispatchID.x;
	if (fn_index >= fn_count)
		return;
	uint meshlet_index = rwFalseNegative[cbMeshlet.falseNegativeIndexOffset + fn_index];

	MeshletBound bound = MeshletBounds[meshlet_index];

	float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, cbMesh.mtxLocalToWorld);
	float3 aabbMin, aabbMax;
	if (!ToScreenAABB(bound, mtxLocalToProj, cbScene.screenInfo.x, cbScene.screenInfo.y, aabbMin, aabbMax))
	{
		bool CullOcclusion = IsOcclusionCull(aabbMin, aabbMax, cbScene.screenInfo.zw * 0.5, HiZ, HIZ_MIP_LEVEL - 1, float2(0, 1));
		//bool CullOcclusion = IsOcclusionCull(aabbMin, aabbMax, cbScene.screenInfo.zw * 0.5, HiZ, 0, float2(0, 1));
		if (CullOcclusion)
		{
			return;
		}
	}

	// create draw arg.
	MeshletDrawInfo info = MeshletDrawInfos[meshlet_index];
	DrawArg arg;
	arg.IndexCountPerInstance = info.indexCount;
	arg.InstanceCount = 1;
	arg.StartIndexLocation = info.indexOffset;
	arg.BaseVertexLocation = 0;
	arg.StartInstanceLocation = 0;

	// count up.
	uint index1, index2;
	rwCounter.InterlockedAdd(cbMeshlet.indirectCount1stByteOffset, 1, index1);
	rwDrawArgs[cbMeshlet.indirectArg1stIndexOffset + index1] = arg;
	rwCounter.InterlockedAdd(cbMeshlet.indirectCount2ndByteOffset, 1, index2);
	rwDrawArgs[cbMeshlet.indirectArg2ndIndexOffset + index2] = arg;

	uint p;
	rwDrawCounter.InterlockedAdd(0, 1, p);
}
