#ifndef CONSTANT_H
#define CONSTANT_H

#ifdef USE_IN_CPP
#	define		float4x4		DirectX::XMFLOAT4X4
#	define		float4			DirectX::XMFLOAT4
#	define		float3			DirectX::XMFLOAT3
#	define		float2			DirectX::XMFLOAT2
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

struct FrustumCB
{
	float4		frustumPlanes[6];
};

struct MeshCB
{
	float4x4	mtxLocalToWorld;
	float4x4	mtxPrevLocalToWorld;
};

#define PI		3.1415926

#endif // CONSTANT_H
//	EOF
