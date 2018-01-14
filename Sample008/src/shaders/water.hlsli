#ifndef SHADERS_WATER_HLSLI
#define SHADERS_WATER_HLSLI

#include "const_buffer.hlsli"

struct VSInput
{
	float4	position	: POSITION;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float4	posCopy		: TEXCOORD0;
};

struct PSOutput
{
	float4	color		: SV_TARGET0;
};

Texture2D		texSSPR;
SamplerState	samLinear;

VSOutput mainVS(VSInput In)
{
	VSOutput Out;

	Out.position = Out.posCopy = mul(mtxViewToClip, mul(mtxWorldToView, mul(mtxLocalToWorld, In.position)));

	return Out;
}

PSOutput mainPS(VSOutput In)
{
	PSOutput Out;

	float2 uv = In.posCopy.xy / In.posCopy.w * float2(0.5, -0.5) + 0.5;
	Out.color = texSSPR.SampleLevel(samLinear, uv, 0.0);

	return Out;
}

#endif // SHADERS_WATER_HLSLI
//	EOF
