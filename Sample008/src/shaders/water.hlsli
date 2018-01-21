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

	float4 posWS = In.position;
	posWS.y = waterHeight;
	float4 posVS = mul(mtxWorldToView, posWS);
	Out.position = Out.posCopy = mul(mtxViewToClip, posVS);
	Out.viewDirWS = -mul((float3x3)mtxViewToWorld, posVS.xyz);
	Out.normalUV = posWS.zx / float2(400.0, 400.0);

	return Out;
}

PSOutput mainPS(VSOutput In)
{
	PSOutput Out;

	float3 V = normalize(In.viewDirWS);
	float3 N = normalize(texNormal.Sample(samLinear, In.normalUV).xzy * 2.0 - 1.0);
	float3 R = 2.0 * dot(N, V) * N - V;
	float3 R_VS = mul((float3x3)mtxWorldToView, R);

	// ƒtƒŒƒlƒ‹‚É‚æ‚é”½ŽË—Ê‚ð‹‚ß‚é
	float f = pow(1.0 - dot(V, N), fresnelCoeff);
	float t = lerp(0.08, 1.0, f);

	float2 uv = In.posCopy.xy / In.posCopy.w * float2(0.5, -0.5) + 0.5;
	if (enableFresnel > 0.0)
	{
		Out.color = texSSPR.SampleLevel(samLinear, uv + R_VS.xy * 0.02, 0.0);
		Out.color.rgb = lerp(0.0, Out.color.rgb, t);
		Out.color.a = t;
	}
	else
	{
		Out.color = texSSPR.SampleLevel(samLinear, uv, 0.0);
	}

	return Out;
}

#endif // SHADERS_WATER_HLSLI
//	EOF
