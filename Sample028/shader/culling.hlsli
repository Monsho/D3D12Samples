#ifndef CULLING_HLSLI
#define CULLING_HLSLI

#include "common.hlsli"

// meshlet bounding data structure.
struct MeshletBound
{
	float3		aabbMin;
	float3		aabbMax;
	float3		coneApex;
	float3		coneAxis;
	float		coneCutoff;
	uint		pad[3];
};	// struct MeshletBound

struct MeshletDrawInfo
{
	uint		indexOffset;
	uint		indexCount;
	uint		pad[2];
};	// struct MeshletDrawInfo

struct MeshletCB
{
	uint		meshletCount;
	uint		indirectArg1stIndexOffset;
	uint		indirectArg2ndIndexOffset;
	uint		indirectCount1stByteOffset;
	uint		indirectCount2ndByteOffset;
	uint		falseNegativeIndexOffset;
	uint		falseNegativeCountByteOffset;
};	// struct MeshletCB

// test cull by frustum.
//   true : this meshlet is invisible.
//   false : this meshlet is visible.
bool IsFrustumCull(in MeshletBound meshlet, in float4 frustumPlanes[6], in float4x4 mtxLocalToWorld)
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
		points[pointID] = mul(mtxLocalToWorld, points[pointID]);

	for (int planeID = 0; planeID < 6; planeID++)
	{
		float3 plane_normal = frustumPlanes[planeID].xyz;
		float plane_constant = frustumPlanes[planeID].w;

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

// test cull backface meshlet.
//   true : this meshlet is invisible.
//   false : this meshlet is visible.
bool IsBackfaceCull(in MeshletBound meshlet, in float3 camPos, in float4x4 mtxLocalToWorld)
{
	float3 coneApex = mul(mtxLocalToWorld, float4(meshlet.coneApex, 1)).xyz;
	return dot(normalize(coneApex - camPos), meshlet.coneAxis) >= meshlet.coneCutoff;
}

// to screen AABB.
//   true : collide near clip plane.
//   false : not collide near clip plane.
bool ToScreenAABB(in MeshletBound meshlet, in float4x4 mtxLocalToProj, in float nearZ, in float farZ, out float3 aabbMin, out float3 aabbMax)
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

	int pointID;
	for (pointID = 0; pointID < 8; pointID++)
	{
		points[pointID] = mul(mtxLocalToProj, points[pointID]);
		float iw = 1.0 / points[pointID].w;
		points[pointID].xy = (points[pointID].xy * iw) * float2(0.5, -0.5) + 0.5;
	}

	aabbMin = aabbMax = points[0].xyw;
	for (pointID = 1; pointID < 8; pointID++)
	{
		aabbMin = min(aabbMin, points[pointID].xyw);
		aabbMax = max(aabbMax, points[pointID].xyw);
	}
	if (aabbMin.z <= nearZ)
		return true;

	aabbMin.xy = clamp(aabbMin.xy, 0.0, 1.0);
	aabbMin.z = clamp(aabbMin.z, nearZ, farZ);
	aabbMax.xy = clamp(aabbMax.xy, 0.0, 1.0);
	aabbMax.z = clamp(aabbMax.z, nearZ, farZ);
	return false;
}

// test cull HiZ.
//   true : this aabb is invisible.
//   false : this aabb is visible.
bool IsOcclusionCull(in float3 aabbMin, in float3 aabbMax, in float2 hizSize, in Texture2D<float2> texHiZ, in uint mipLevel, in float2 maskMinMax)
{
	float4 rect = float4(aabbMin.xy, aabbMax.xy) * hizSize.xyxy;
	uint maxTexel = (uint)max(rect.z - rect.x, rect.w - rect.y) + 1;
	uint desiredMip = min(firstbithigh(maxTexel), mipLevel);
	
	float2 levelSize = hizSize / exp2(desiredMip);
	float2 left_top = aabbMin.xy * levelSize;
	float2 right_bottom = aabbMax.xy * levelSize;
	uint startX = (uint)left_top.x;
	uint startY = (uint)left_top.y;
	uint endX = (uint)right_bottom.x + (frac(right_bottom.x) > 0.0 ? 1 : 0);
	uint endY = (uint)right_bottom.y + (frac(right_bottom.y) > 0.0 ? 1 : 0);
	float rectZ = aabbMin.z;

	float maxZ = 0.0;
	for (uint y = startY; y <= endY; y++)
	{
		for (uint x = startX; x <= endX; x++)
		{
			uint3 uvw = uint3(x, y, desiredMip);
			maxZ = max(maxZ, dot(texHiZ.Load(uvw), maskMinMax));
		}
	}
	return rectZ > maxZ;
}

#endif // CULLING_HLSLI
