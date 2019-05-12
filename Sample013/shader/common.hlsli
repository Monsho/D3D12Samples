#ifndef COMMON_HLSLI
#define COMMON_HLSLI

struct SceneCB
{
	float4x4	mtxWorldToProj;
	float4x4	mtxProjToWorld;
	float4		camPos;
	float4		lightDir;
	float4		lightColor;
	float		skyPower;
	uint		maxBounces;
};

struct TimeCB
{
	uint		loopCount;
};

float3 SkyColor(float3 w_dir)
{
	float3 t = w_dir.y * 0.5 + 0.5;
	return saturate((1.0).xxx * (1.0 - t) + float3(0.5, 0.7, 1.0) * t);
}

#endif // COMMON_HLSLI
//	EOF
