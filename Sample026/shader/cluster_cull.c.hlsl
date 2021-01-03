#include "common.hlsli"

ConstantBuffer<SceneCB>				cbScene			: register(b0);

StructuredBuffer<PointLightPos>		rLightPosBuffer	: register(t0);

RWStructuredBuffer<ClusterInfo>		rwClusterInfo	: register(u0);

// get cluster frustom planes in view space.
void GetClusterFrustumPlanes(out float4 frustumPlanes[6], uint2 xy, uint z)
{
	float nearZ = cbScene.screenInfo.x;
	float farZ = cbScene.screenInfo.y;
	float clusterZSize = (farZ - nearZ) / (float)CLUSTER_DIV_Z;
	float minZ = nearZ + (clusterZSize * (float)z);
	float maxZ = minZ + clusterZSize;

	float2 tileScale = (float)CLUSTER_DIV_XY * 0.5;
	float2 tileBias = tileScale - float2(xy);

	float4 c1 = float4(cbScene.mtxViewToProj._11 * tileScale.x, 0.0, -tileBias.x, 0.0);
	float4 c2 = float4(0.0, -cbScene.mtxViewToProj._22 * tileScale.y, -tileBias.y, 0.0);
	float4 c4 = float4(0.0, 0.0, -1.0, 0.0);

	frustumPlanes[0] = c4 - c1;		// Right
	frustumPlanes[1] = c1;			// Left
	frustumPlanes[2] = c4 - c2;		// Top
	frustumPlanes[3] = c2;			// Bottom
	frustumPlanes[4] = float4(0.0, 0.0, -1.0, -minZ);
	frustumPlanes[5] = float4(0.0, 0.0, 1.0, maxZ);

	for (uint i = 0; i < 4; ++i)
	{
		frustumPlanes[i] *= rcp(length(frustumPlanes[i].xyz));
	}
}

[numthreads(CLUSTER_DIV_XY, CLUSTER_DIV_XY, 1)]
void main(
	uint3 dtid : SV_DispatchThreadID)
{
	uint infoIndex = dtid.z * CLUSTER_DIV_XY * CLUSTER_DIV_XY + dtid.y * CLUSTER_DIV_XY + dtid.x;

	// get frustum planes.
	float4 frustumPlanes[6];
	GetClusterFrustumPlanes(frustumPlanes, dtid.xy, dtid.z);

	// culling lights.
	ClusterInfo info = (ClusterInfo)0;
	for (uint flagIndex = 0; flagIndex < 4; flagIndex++)
	{
		info.lightFlags[flagIndex] = 0;
		uint baseIndex = flagIndex * 32;
		for (uint bitIndex = 0; bitIndex < 32; bitIndex++)
		{
			// get light position in view space.
			PointLightPos light = rLightPosBuffer[baseIndex + bitIndex];
			float3 posInView = mul(cbScene.mtxWorldToView, float4(light.posAndRadius.xyz, 1)).xyz;
			float radius = light.posAndRadius.w;

			// frustum culling.
			bool inFrustum = true;
			[unroll]
			for (uint planeIndex = 0; planeIndex < 6; planeIndex++)
			{
				float d = dot(frustumPlanes[planeIndex], float4(posInView, 1));
				inFrustum = inFrustum && (d >= -radius);
			}

			info.lightFlags[flagIndex] |= inFrustum ? (0x01 << bitIndex) : 0x0;
		}
	}

	rwClusterInfo[infoIndex] = info;
}
