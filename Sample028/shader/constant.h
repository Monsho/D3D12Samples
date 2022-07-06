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
	float3		viewDepthValue;			// x: m33, y: m43, z: m34
	uint		isOcclusionCull;
	uint		isMeshletColor;
	uint		renderColorSpace;
	uint		frameIndex;
};

struct LightCB
{
	float4		lightDir;
	float4		lightColor;
	float		skyPower;
	float		giIntensity;
};

struct MeshMaterialCB
{
	float4		baseColor;
	float3		emissiveColor;
	float		pad;
	float		roughness;
	float		metallic;
};

struct MaterialCB
{
	float2		roughnessRange;
	float2		metallicRange;
	float		emissiveIntensity;
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

struct TranslucentCB
{
	float		normalIntensity;
	float		opacity;
	float		refract;
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

struct VertexMutationCB
{
	uint	vertexCount;
	float	mutateIntensity;
	float	time;
};

struct TonemapCB
{
	uint	type;
	float	baseLuminance;
	float	maxLuminance;
	uint	renderColorSpace;
};

struct UIDrawCB
{
	float4	rect;
	float	alpha;
	float	intensity;
	uint	blendType;			// 0: translucent, 1: additive
	uint	colorSpace;			// 0: Rec709, 1: Rec709+OETF, 2: Rec2020, 3: Rec2020+OETF
};

struct RayData
{
	float3	origin;
	float3	direction;
	uint	packedPixelPos;
	uint	flag;
};


#define PI				3.1415926
#define Epsilon			1e-5
#define kGoldenRatio	1.61803398875

#define CLUSTER_DIV_XY		16
#define CLUSTER_DIV_Z		16

#define HIZ_MIP_LEVEL		6

#define ENABLE_POINT_LIGHTS		0

#endif // CONSTANT_H
//	EOF
