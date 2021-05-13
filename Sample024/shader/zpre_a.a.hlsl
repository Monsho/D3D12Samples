#include "common.hlsli"

struct Payload
{
	uint MeshletIndices[LANE_COUNT_IN_WAVE];
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD;

	float4	currPosCS	: CURR_POS_CS;
	float4	prevPosCS	: PREV_POS_CS;
};

struct Meshlet
{
	float3		aabbMin;
	uint		primitiveOffset;
	float3		aabbMax;
	uint		primitiveCount;
	float3		coneApex;
	uint		vertexIndexOffset;
	float3		coneAxis;
	uint		vertexIndexCount;
	float		coneCutoff;
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

bool IsBackfaceCull(float3 cone_apex, float3 cone_axis, float cone_cutoff, float3 cam_pos)
{
	cone_apex = mul(cbMesh.mtxLocalToWorld, float4(cone_apex, 1)).xyz;
	return dot(normalize(cone_apex - cam_pos), cone_axis) >= cone_cutoff;
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

		if (!cbScene.isFrustumCull
			|| (!IsFrustumCull(ml.aabbMin, ml.aabbMax)
			&& !IsBackfaceCull(ml.coneApex, ml.coneAxis, ml.coneCutoff, cbFrustum.camPos.xyz)))
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
