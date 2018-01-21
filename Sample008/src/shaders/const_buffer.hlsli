#ifndef SHADERS_CONST_BUFFER_HLSLI
#define SHADERS_CONST_BUFFER_HLSLI

cbuffer CbScene
{
	float4x4	mtxWorldToView;
	float4x4	mtxViewToClip;
	float4x4	mtxViewToWorld;
	float4x4	mtxPrevWorldToClip;
	float4		screenInfo;			// (ScreenWidth, ScreenHeight, nearZ, farZ)
	float4		frustumCorner;
};

cbuffer CbMesh
{
	float4x4	mtxLocalToWorld;
};

cbuffer CbWaterInfo
{
	float		waterHeight;
	float		temporalBlend;
	float		enableFresnel;
	float		fresnelCoeff;
};

#endif // SHADERS_CONST_BUFFER_HLSLI
