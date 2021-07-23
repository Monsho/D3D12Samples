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
	uint meshlet_index = dispatchID.x;
	if (meshlet_index >= cbMeshlet.meshletCount)
		return;

	MeshletBound bound = MeshletBounds[meshlet_index];
	bool CullFrustum = IsFrustumCull(bound, cbFrustum.frustumPlanes, cbMesh.mtxLocalToWorld);
	bool CullBackface = IsBackfaceCull(bound, cbScene.camPos.xyz, cbMesh.mtxLocalToWorld);

	[branch]
	if (!CullFrustum && !CullBackface)
	{
		// occlusion culling.
		if (cbScene.isOcclusionCull)
		{
			float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, cbMesh.mtxLocalToWorld);
			float3 aabbMin, aabbMax;
			if (!ToScreenAABB(bound, mtxLocalToProj, cbScene.screenInfo.x, cbScene.screenInfo.y, aabbMin, aabbMax))
			{
				bool CullOcclusion = IsOcclusionCull(aabbMin, aabbMax, cbScene.screenInfo.zw * 0.5, HiZ, HIZ_MIP_LEVEL - 1, float2(1, 0));
				if (CullOcclusion)
				{
					uint fn_index;
					rwFNCounter.InterlockedAdd(cbMeshlet.falseNegativeCountByteOffset, 1, fn_index);
					rwFalseNegative[cbMeshlet.falseNegativeIndexOffset + fn_index] = meshlet_index;
					return;
				}
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
		uint arg_index;
		rwCounter.InterlockedAdd(cbMeshlet.indirectCount1stByteOffset, 1, arg_index);
		rwDrawArgs[cbMeshlet.indirectArg1stIndexOffset + arg_index] = arg;

		uint p;
		rwDrawCounter.InterlockedAdd(0, 1, p);
	}
}
