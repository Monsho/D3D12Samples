#ifndef SHADERS_BASE_PASS_HLSLI
#define SHADERS_BASE_PASS_HLSLI

#include "const_buffer.hlsli"

struct VSInput
{
	float4	position	: POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD0;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	normalWS	: NORMAL;
	float2	uv			: TEXCOORD0;
};

struct PSOutput
{
	float4	normalWS	: SV_TARGET0;
	float4	baseColor	: SV_TARGET1;
	float4	metalRough	: SV_TARGET2;
};

VSOutput mainVS(VSInput In)
{
	VSOutput Out;

	Out.position = mul(mtxViewToClip, mul(mtxWorldToView, mul(mtxLocalToWorld, In.position)));
	Out.normalWS = normalize(mul((float3x3)mtxLocalToWorld, In.normal));
	Out.uv = In.uv;

	return Out;
}

PSOutput mainPS(VSOutput In)
{
	PSOutput Out;

	Out.normalWS = float4(In.normalWS, 1);
	Out.baseColor = float4(0.5, 0.5, 0.5, 1.0);
	Out.metalRough = float4(0, 0.5, 0, 0);

	return Out;
}

#endif // SHADERS_BASE_PASS_HLSLI
//	EOF
