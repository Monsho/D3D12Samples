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
	float4	posCopy		: POS_COPY;
	float3	viewDirWS	: VIEW_DIR_WS;
	float2	normalUV	: NORMAL_UV;
};

struct PSOutput
{
	float4	color		: SV_TARGET0;
};

Texture2D		texSSPR;
Texture2D		texNormal;
SamplerState	samLinear;

VSOutput mainVS(VSInput In)
{
	VSOutput Out;

	float4 posVS = mul(mtxWorldToView, In.position);
	Out.position = Out.posCopy = mul(mtxViewToClip, posVS);
	Out.viewDirWS = -mul((float3x3)mtxViewToWorld, posVS.xyz);
	Out.normalUV = In.position.zx / float2(500.0, 500.0);

	return Out;
}

PSOutput mainPS(VSOutput In)
{
	PSOutput Out;

	float3 V = normalize(In.viewDirWS);
	float3 N = normalize(texNormal.Sample(samLinear, In.normalUV).xzy * 2.0 - 1.0);
	float3 N_VS = mul((float3x3)mtxWorldToView, N);

	// ÉtÉåÉlÉãÇ…ÇÊÇÈîΩéÀó ÇãÅÇﬂÇÈ
	float f = pow(1.0 - dot(V, N), 3.0);
	float t = lerp(0.08, 1.0, f);

	float2 uv = In.posCopy.xy / In.posCopy.w * float2(0.5, -0.5) + 0.5;
	Out.color = texSSPR.SampleLevel(samLinear, uv + N_VS.xy * 0.01, 0.0);
	Out.color.rgb = lerp(0.0, Out.color.rgb, t);

	return Out;
}

#endif // SHADERS_WATER_HLSLI
//	EOF
