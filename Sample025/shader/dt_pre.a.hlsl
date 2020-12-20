#include "common.hlsli"

struct Payload
{
	uint MeshletIndices[LANE_COUNT_IN_WAVE];
};

struct Meshlet
{
	float3		aabbMin;
	uint		primitiveOffset;
	float3		aabbMax;
	uint		primitiveCount;
	uint		vertexIndexOffset;
	uint		vertexIndexCount;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
ConstantBuffer<FrustumCB>	cbFrustum		: register(b1);
ConstantBuffer<MeshCB>		cbMesh			: register(b2);

StructuredBuffer<Meshlet>	inputMeshlets	: register(t0);

bool IsFrustumCull(float3 aabbMin, float3 aabbMax)
{
	float4 points[8];
	points[0] = float4(aabbMin.x, aabbMin.y, aabbMin.z, 1);
	points[1] = float4(aabbMin.x, aabbMin.y, aabbMax.z, 1);
	points[2] = float4(aabbMin.x, aabbMax.y, aabbMin.z, 1);
	points[3] = float4(aabbMin.x, aabbMax.y, aabbMax.z, 1);
	points[4] = float4(aabbMax.x, aabbMin.y, aabbMin.z, 1);
	points[5] = float4(aabbMax.x, aabbMin.y, aabbMax.z, 1);
	points[6] = float4(aabbMax.x, aabbMax.y, aabbMin.z, 1);
	points[7] = float4(aabbMax.x, aabbMax.y, aabbMax.z, 1);

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

groupshared Payload sPayload;

[NumThreads(LANE_COUNT_IN_WAVE, 1, 1)]
void main(
	uint gtid : SV_GroupThreadID,
	uint dtid : SV_DispatchThreadID,
	uint gid : SV_GroupID
)
{
	uint num, stride;
	inputMeshlets.GetDimensions(num, stride);

	bool visible = false;
	if (dtid < num)
	{
		Meshlet	ml = inputMeshlets[dtid];

		if (!cbScene.isFrustumCull || !IsFrustumCull(ml.aabbMin, ml.aabbMax))
		{
			visible = true;
		}
	}

	if (visible)
	{
		uint index = WavePrefixCountBits(visible);
		sPayload.MeshletIndices[index] = dtid;
	}

	uint visible_count = WaveActiveCountBits(visible);
	DispatchMesh(visible_count, 1, 1, sPayload);
}

//	EOF
