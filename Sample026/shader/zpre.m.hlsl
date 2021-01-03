#include "common.hlsli"

struct Payload
{
	uint MeshletIndices[LANE_COUNT_IN_WAVE];
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
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
StructuredBuffer<float3>	vertexPosition	: register(t1);
StructuredBuffer<float3>	vertexNormal	: register(t2);
StructuredBuffer<float2>	vertexTexcoord	: register(t3);
StructuredBuffer<uint>		primitiveIndex	: register(t4);
StructuredBuffer<uint>		vertexIndex		: register(t5);

uint3 UnpackPrimitiveIndex(uint index)
{
	return uint3(
		(index >> 0) & 0x3ff,
		(index >> 10) & 0x3ff,
		(index >> 20) & 0x3ff);
}

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

[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void main(
	uint gtid : SV_GroupThreadID,
	uint gid : SV_GroupID,
	in payload Payload payload,
	out indices uint3 tris[126],
	out vertices VSOutput verts[64]
)
{
	uint meshletIndex = payload.MeshletIndices[gid];
	Meshlet	ml = inputMeshlets[meshletIndex];
	uint vcount = ml.vertexIndexCount;
	uint pcount = ml.primitiveCount;

	SetMeshOutputCounts(vcount, pcount);

	if (gtid < pcount)
	{
		tris[gtid] = UnpackPrimitiveIndex(primitiveIndex[ml.primitiveOffset + gtid]);
	}

	if (gtid < vcount)
	{
		VSOutput Out;

		float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, cbMesh.mtxLocalToWorld);
		float4x4 mtxPrevLocalToProj = mul(cbScene.mtxPrevWorldToProj, cbMesh.mtxPrevLocalToWorld);

		uint index = vertexIndex[ml.vertexIndexOffset + gtid];
		float3 InPos = vertexPosition[index];
		float2 InUV = vertexTexcoord[index];
		Out.position = mul(mtxLocalToProj, float4(InPos, 1));
		Out.uv = InUV;

		verts[gtid] = Out;
	}
}

//	EOF
