#include "common.hlsli"
#include "culling.hlsli"

struct CullCB
{
	uint		CullUnitCount;
	uint		BatchCount;
};

struct DrawArg
{
	uint		IndexCountPerInstance;
	uint		InstanceCount;
	uint		StartIndexLocation;
	int			BaseVertexLocation;
	uint		StartInstanceLocation;
};

struct CullUnitData
{
	uint		InstanceIndex;
	uint		BatchIndex;
	uint		MeshletIndex;
	uint		pad;
};

struct InstanceData
{
	float4x4	Transform;
};

struct BatchData
{
	uint		IndirectArgOffset;
	uint		MeshletOffset;
	uint		pad[2];
};

ConstantBuffer<SceneCB>		cbScene				: register(b0);
ConstantBuffer<FrustumCB>	cbFrustum			: register(b1);
ConstantBuffer<CullCB>		cbCull				: register(b2);

StructuredBuffer<CullUnitData>		CullUnits			: register(t0);
StructuredBuffer<InstanceData>		Instances			: register(t1);
StructuredBuffer<BatchData>			Batches				: register(t2);
StructuredBuffer<MeshletBound>		MeshletBounds		: register(t3);
StructuredBuffer<MeshletDrawInfo>	MeshletDrawInfos	: register(t4);
Texture2D<float2>					HiZ					: register(t5);

RWByteAddressBuffer			rwIndirectArgs		: register(u0);
RWByteAddressBuffer			rwDrawCount			: register(u1);
RWByteAddressBuffer			rwFalseNegative		: register(u2);
RWByteAddressBuffer			rwFNIndirectArg		: register(u3);
RWByteAddressBuffer			rwTotalCount		: register(u4);

void StoreDrawArg(uint index, DrawArg arg)
{
	uint address = 20 * index;
	rwIndirectArgs.Store(address + 0, arg.IndexCountPerInstance);
	rwIndirectArgs.Store(address + 4, arg.InstanceCount);
	rwIndirectArgs.Store(address + 8, arg.StartIndexLocation);
	rwIndirectArgs.Store(address + 12, arg.BaseVertexLocation);
	rwIndirectArgs.Store(address + 16, arg.StartInstanceLocation);
}

[numthreads(32, 1, 1)]
void ClearCountCS(uint3 dispatchID : SV_DispatchThreadID)
{
	uint address = dispatchID.x;
	if (cbCull.BatchCount * 2 + 1 <= address)
		return;

	rwDrawCount.Store(address * 4, 0);
	if (address == 0)
		rwTotalCount.Store(0, 0);
}

[numthreads(1, 1, 1)]
void MakeFNDispatchCS()
{
	uint countOffset = cbCull.BatchCount * 2;
	uint fnCount = rwDrawCount.Load(countOffset * 4);
	uint dispatchCount = (fnCount + 31) / 32;

	rwFNIndirectArg.Store(0, dispatchCount);
	rwFNIndirectArg.Store(4, 1);
	rwFNIndirectArg.Store(8, 1);
}

[numthreads(32, 1, 1)]
void Cull1stPhaseCS(uint3 dispatchID : SV_DispatchThreadID)
{
	uint cullUnitIndex = dispatchID.x;
	if (cbCull.CullUnitCount <= cullUnitIndex)
		return;

	CullUnitData cullUnit = CullUnits[cullUnitIndex];
	InstanceData instance = Instances[cullUnit.InstanceIndex];
	BatchData batch = Batches[cullUnit.BatchIndex];
	uint meshletIndex = batch.MeshletOffset + cullUnit.MeshletIndex;
	MeshletBound bound = MeshletBounds[meshletIndex];
	bool CullFrustum = IsFrustumCull(bound, cbFrustum.frustumPlanes, instance.Transform);
	bool CullBackface = IsBackfaceCull(bound, cbScene.camPos.xyz, instance.Transform);

	[branch]
	if (!CullFrustum && !CullBackface)
	{
		// occlusion culling.
		if (cbScene.isOcclusionCull)
		{
			float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, instance.Transform);
			float3 aabbMin, aabbMax;
			if (!ToScreenAABB(bound, mtxLocalToProj, cbScene.screenInfo.x, cbScene.screenInfo.y, aabbMin, aabbMax))
			{
				bool CullOcclusion = IsOcclusionCull(aabbMin, aabbMax, cbScene.screenInfo.zw * 0.5, HiZ, HIZ_MIP_LEVEL - 1, float2(1, 0));
				if (CullOcclusion)
				{
					uint countOffset = cbCull.BatchCount * 2;
					uint fnIndex;
					rwDrawCount.InterlockedAdd(countOffset * 4, 1, fnIndex);
					rwFalseNegative.Store(fnIndex * 4, cullUnitIndex);
					return;
				}
			}
		}

		// create indirect arg.
		MeshletDrawInfo info = MeshletDrawInfos[meshletIndex];
		DrawArg arg = (DrawArg)0;
		arg.IndexCountPerInstance = info.indexCount;
		arg.InstanceCount = 1;
		arg.StartIndexLocation = info.indexOffset;
		arg.BaseVertexLocation = 0;
		arg.StartInstanceLocation = 0;

		// store indirect arg.
		uint argIndex;
		rwDrawCount.InterlockedAdd(cullUnit.BatchIndex * 4, 1, argIndex);
		StoreDrawArg(batch.IndirectArgOffset + argIndex, arg);

		uint p;
		rwTotalCount.InterlockedAdd(0, 1, p);
	}
}

[numthreads(32, 1, 1)]
void Cull2ndPhaseCS(uint3 dispatchID : SV_DispatchThreadID)
{
	// get false negative index.
	uint countOffset = cbCull.BatchCount * 2;
	uint fnCount = rwDrawCount.Load(countOffset * 4);
	uint fnIndex = dispatchID.x;
	if (fnCount <= fnIndex)
		return;

	uint cullUnitIndex = rwFalseNegative.Load(fnIndex * 4);
	CullUnitData cullUnit = CullUnits[cullUnitIndex];
	InstanceData instance = Instances[cullUnit.InstanceIndex];
	BatchData batch = Batches[cullUnit.BatchIndex];
	uint meshletIndex = batch.MeshletOffset + cullUnit.MeshletIndex;
	MeshletBound bound = MeshletBounds[meshletIndex];

	float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, instance.Transform);
	float3 aabbMin, aabbMax;
	if (!ToScreenAABB(bound, mtxLocalToProj, cbScene.screenInfo.x, cbScene.screenInfo.y, aabbMin, aabbMax))
	{
		bool CullOcclusion = IsOcclusionCull(aabbMin, aabbMax, cbScene.screenInfo.zw * 0.5, HiZ, HIZ_MIP_LEVEL - 1, float2(0, 1));
		if (CullOcclusion)
		{
			return;
		}
	}

	// create indirect arg.
	MeshletDrawInfo info = MeshletDrawInfos[meshletIndex];
	DrawArg arg = (DrawArg)0;
	arg.IndexCountPerInstance = info.indexCount;
	arg.InstanceCount = 1;
	arg.StartIndexLocation = info.indexOffset;
	arg.BaseVertexLocation = 0;
	arg.StartInstanceLocation = 0;

	// store indirect arg.
	uint index1, index2;
	rwDrawCount.InterlockedAdd(cullUnit.BatchIndex * 4, 1, index1);
	StoreDrawArg(batch.IndirectArgOffset + index1, arg);
	rwDrawCount.InterlockedAdd((cbCull.BatchCount + cullUnit.BatchIndex) * 4, 1, index2);
	StoreDrawArg(cbCull.CullUnitCount + batch.IndirectArgOffset + index2, arg);

	uint p;
	rwTotalCount.InterlockedAdd(0, 1, p);
}
