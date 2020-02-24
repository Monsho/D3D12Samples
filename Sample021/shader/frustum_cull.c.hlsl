#include "common.hlsli"

struct Meshlet
{
#if 1
	float3		aabbMin;
	uint		indexOffset;
	float3		aabbMax;
	uint		indexCount;
#else
	float4		centerAndRadius;
	uint		indexOffset;
	uint		indexCount;
#endif
};

struct DrawArg
{
	uint		IndexCountPerInstance;
	uint		InstanceCount;
	uint		StartIndexLocation;
	int			BaseVertexLocation;
	uint		StartInstanceLocation;
};

ConstantBuffer<FrustumCB>	cbFrustum			: register(b0);
ConstantBuffer<MeshCB>		cbMesh				: register(b1);

StructuredBuffer<Meshlet>	inputMeshlets		: register(t0);

RWStructuredBuffer<DrawArg>	outputDrawArgs		: register(u0);
RWByteAddressBuffer			outputCounter		: register(u1);

bool IsOutsidePlane(float4 plane, float3 center, float radius)
{
	float d = dot(plane, float4(center, 1));
	return d <= -radius;
}

#if 0
// ä»à’Frustum Culling
bool IsFrustumCull(Meshlet meshlet)
{
	float4 points[8];
	points[0] = float4(meshlet.aabbMin.x, meshlet.aabbMin.y, meshlet.aabbMin.z, 1);
	points[1] = float4(meshlet.aabbMin.x, meshlet.aabbMin.y, meshlet.aabbMax.z, 1);
	points[2] = float4(meshlet.aabbMin.x, meshlet.aabbMax.y, meshlet.aabbMin.z, 1);
	points[3] = float4(meshlet.aabbMin.x, meshlet.aabbMax.y, meshlet.aabbMax.z, 1);
	points[4] = float4(meshlet.aabbMax.x, meshlet.aabbMin.y, meshlet.aabbMin.z, 1);
	points[5] = float4(meshlet.aabbMax.x, meshlet.aabbMin.y, meshlet.aabbMax.z, 1);
	points[6] = float4(meshlet.aabbMax.x, meshlet.aabbMax.y, meshlet.aabbMin.z, 1);
	points[7] = float4(meshlet.aabbMax.x, meshlet.aabbMax.y, meshlet.aabbMax.z, 1);

	for (int pointID = 0; pointID < 8; pointID++)
		points[pointID] = mul(cbMesh.mtxLocalToWorld, points[pointID]);

	for (int planeID = 0; planeID < 6; planeID++)
	{
		float3 plane_normal = cbFrustum.frustumPlanes[planeID].xyz;
		float plane_constant = cbFrustum.frustumPlanes[planeID].w;

		bool inside = false;
		for (int pointID = 0; pointID < 8; pointID++)
		{
			if (dot(plane_normal, points[pointID].xyz) + plane_constant >= 0)
			{
				inside = true;
				break;
			}
		}

		if (!inside)
		{
			return true;
		}
	}

	return false;
}

[numthreads(1, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint meshlet_index = dispatchID.x;
	Meshlet meshlet = inputMeshlets[meshlet_index];

	if (!IsFrustumCull(meshlet))
	{
		// Frustumì‡Ç…ë∂ç›Ç∑ÇÈèÍçáÇÕDrawArgÇèëÇ´çûÇﬁ
		DrawArg arg;
		arg.IndexCountPerInstance = meshlet.indexCount;
		arg.InstanceCount = 1;
		arg.StartIndexLocation = meshlet.indexOffset;
		arg.BaseVertexLocation = 0;
		arg.StartInstanceLocation = 0;

		uint arg_index = 0;
		outputCounter.InterlockedAdd(0, 1, arg_index);
		outputDrawArgs[arg_index] = arg;
	}
}

#else
// ä»à’Frustum Culling
//bool IsFrustumCull(Meshlet meshlet)
//{
//	float3 mc = mul(cbMesh.mtxLocalToWorld, float4(meshlet.centerAndRadius.xyz, 1)).xyz;
//	float radius = meshlet.centerAndRadius.w;
//	radius *= max(length(cbMesh.mtxLocalToWorld[0].xyz), max(length(cbMesh.mtxLocalToWorld[1].xyz), length(cbMesh.mtxLocalToWorld[2].xyz)));
//
//	for (int planeID = 0; planeID < 6; planeID++)
//	{
//		if (IsOutsidePlane(cbFrustum.frustumPlanes[planeID], mc, radius))
//		{
//			return true;
//		}
//	}
//
//	return false;
//}
bool IsFrustumCull(Meshlet meshlet)
{
	float4 points[8];
	points[0] = float4(meshlet.aabbMin.x, meshlet.aabbMin.y, meshlet.aabbMin.z, 1);
	points[1] = float4(meshlet.aabbMin.x, meshlet.aabbMin.y, meshlet.aabbMax.z, 1);
	points[2] = float4(meshlet.aabbMin.x, meshlet.aabbMax.y, meshlet.aabbMin.z, 1);
	points[3] = float4(meshlet.aabbMin.x, meshlet.aabbMax.y, meshlet.aabbMax.z, 1);
	points[4] = float4(meshlet.aabbMax.x, meshlet.aabbMin.y, meshlet.aabbMin.z, 1);
	points[5] = float4(meshlet.aabbMax.x, meshlet.aabbMin.y, meshlet.aabbMax.z, 1);
	points[6] = float4(meshlet.aabbMax.x, meshlet.aabbMax.y, meshlet.aabbMin.z, 1);
	points[7] = float4(meshlet.aabbMax.x, meshlet.aabbMax.y, meshlet.aabbMax.z, 1);

	for (int pointID = 0; pointID < 8; pointID++)
		points[pointID] = mul(cbMesh.mtxLocalToWorld, points[pointID]);

	for (int planeID = 0; planeID < 6; planeID++)
	{
		float3 plane_normal = cbFrustum.frustumPlanes[planeID].xyz;
		float plane_constant = cbFrustum.frustumPlanes[planeID].w;

		bool inside = false;
		for (int pointID = 0; pointID < 8; pointID++)
		{
			if (dot(plane_normal, points[pointID].xyz) + plane_constant >= 0)
			{
				inside = true;
				break;
			}
		}

		if (!inside)
		{
			return true;
		}
	}

	return false;
}

[numthreads(1, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint meshlet_index = dispatchID.x;
	Meshlet meshlet = inputMeshlets[meshlet_index];

	DrawArg arg = (DrawArg)0;
	if (!IsFrustumCull(meshlet))
	{
		// Frustumì‡Ç…ë∂ç›Ç∑ÇÈèÍçáÇÕDrawArgêÆå`Ç∑ÇÈ
		arg.IndexCountPerInstance = meshlet.indexCount;
		arg.InstanceCount = 1;
		arg.StartIndexLocation = meshlet.indexOffset;
		arg.BaseVertexLocation = 0;
		arg.StartInstanceLocation = 0;
	}
	outputDrawArgs[meshlet_index] = arg;
}

#endif