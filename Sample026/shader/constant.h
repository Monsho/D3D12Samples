#ifndef CONSTANT_H
#define CONSTANT_H

#ifdef USE_IN_CPP
#	define		float4x4		DirectX::XMFLOAT4X4
#	define		float4			DirectX::XMFLOAT4
#	define		float3			DirectX::XMFLOAT3
#	define		float2			DirectX::XMFLOAT2
#	define		uint			UINT
#endif

struct SceneCB
{
	float4x4	mtxWorldToProj;
	float4x4	mtxWorldToView;
	float4x4	mtxViewToProj;
	float4x4	mtxProjToWorld;
	float4x4	mtxPrevWorldToProj;
	float4x4	mtxPrevProjToWorld;
	float4		screenInfo;				// x: NearZ, y: FarZ, zw: ScreenSize
	float4		camPos;
	uint		isFrustumCull;
	uint		isMeshletColor;
};

struct LightCB
{
	float4		lightDir;
	float4		lightColor;
	float		skyPower;
};

struct MaterialCB
{
	float2		roughnessRange;
	float2		metallicRange;
};

struct FrustumCB
{
	float4		frustumPlanes[6];
};

struct MeshCB
{
	float4x4	mtxLocalToWorld;
	float4x4	mtxPrevLocalToWorld;
};

struct GlobalIlluminationCB
{
	float		intensity;
	uint		sampleCount;
	uint		totalSampleCount;
};

struct PointLightPos
{
	float4	posAndRadius;		// xyz: Position, w: Radius
};

struct PointLightColor
{
	float4	color;
};

struct ClusterInfo
{
	uint	lightFlags[4];		// max 128 lights.
};

struct MaterialIdCB
{
	uint	materialId;
};

struct MaterialInfo
{
	uint	baseColorIndex;
	uint	normalMapIndex;
	uint	ormMapIndex;
	uint	pad;
};


#define PI			3.1415926
#define Epsilon		1e-5

#define CLUSTER_DIV_XY		16
#define CLUSTER_DIV_Z		16

#endif // CONSTANT_H
//	EOF
