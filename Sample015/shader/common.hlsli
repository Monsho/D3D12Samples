#ifndef COMMON_HLSLI
#define COMMON_HLSLI

struct SceneCB
{
	float4x4	mtxWorldToProj;
	float4x4	mtxProjToWorld;
	float4x4	mtxPrevWorldToProj;
	float4x4	mtxPrevProjToWorld;
	float4		screenInfo;
	float4		camPos;
	float4		lightDir;
	float4		lightColor;
	float		skyPower;
	float		aoLength;
	uint		aoSampleCount;
	uint		loopCount;
	uint		randomType;
	uint		temporalOn;
	uint		aoOnly;
};

struct MeshCB
{
	float4x4	mtxLocalToWorld;
	float4x4	mtxPrevLocalToWorld;
};

float3 SkyColor(float3 w_dir)
{
	float3 t = w_dir.y * 0.5 + 0.5;
	return saturate((1.0).xxx * (1.0 - t) + float3(0.5, 0.7, 1.0) * t);
}

float ToLinearDepth(float perspDepth, float n, float f)
{
	return -n / ((n - f) * perspDepth + f);
}

#endif // COMMON_HLSLI
//	EOF
