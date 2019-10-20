#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include "constant.h"

float3 SkyColor(float3 w_dir)
{
	float3 t = w_dir.y * 0.5 + 0.5;
	return saturate((1.0).xxx * (1.0 - t) + float3(0.5, 0.7, 1.0) * t);
}

float ToLinearDepth(float perspDepth, float n, float f)
{
	return -n / ((n - f) * perspDepth + f);
}

float4 RotateGridSuperSample(Texture2D tex, SamplerState sam, float2 uv, float bias)
{
	float2 dx = ddx(uv);
	float2 dy = ddy(uv);

	float2 uvOffsets = float2(0.125, 0.375);
	float2 offsetUV = float2(0.0, 0.0);

	float4 col = 0;
	offsetUV = uv + uvOffsets.x * dx + uvOffsets.y * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	offsetUV = uv - uvOffsets.x * dx - uvOffsets.y * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	offsetUV = uv + uvOffsets.y * dx - uvOffsets.x * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	offsetUV = uv - uvOffsets.y * dx + uvOffsets.x * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	col *= 0.25;

	return col;
}

float2 VectorToRadialCoords(in float3 vc)
{
	float3 normalizedCoords = normalize(vc);
	float latitude = acos(normalizedCoords.y);
	float longitude = atan2(normalizedCoords.z, normalizedCoords.x);
	float2 uv = {
		0.5 - (longitude * 0.5 / PI),
		(latitude / PI)
	};
	return uv;
}

#define MaxSample			512

#endif // COMMON_HLSLI
//	EOF
