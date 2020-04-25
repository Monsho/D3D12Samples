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
	float4x4	mtxProjToWorld;
	float4x4	mtxPrevWorldToProj;
	float4x4	mtxPrevProjToWorld;
	float4		screenInfo;
	float4		camPos;
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

struct ReflectionCB
{
	float		intensity;
	float		currentBlendMax;
	float		roughnessMax;
	uint		noiseTexWidth;
	uint		time;
	uint		timeMax;
};

#define PI			3.1415926
#define Epsilon		1e-5

#endif // CONSTANT_H
//	EOF
