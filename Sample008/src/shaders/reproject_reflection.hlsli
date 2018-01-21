#ifndef SHADERS_REPROJECT_REFLECTION_HLSLI
#define SHADERS_REPROJECT_REFLECTION_HLSLI

#include "const_buffer.hlsli"

struct VSInput
{
	float4	position	: POSITION;
};

struct VSOutput
{
	float4	position		: SV_POSITION;
	float4	posCopy			: POS_COPY;
	float4	prevPosition	: PREV_POSITION;
};

struct PSOutput
{
	float4	color		: SV_TARGET0;
};

Texture2D				texPrevReflection;
Texture2D<uint>			texProjectHash;

VSOutput mainVS(VSInput In)
{
	VSOutput Out;

	float4 posWS = In.position;
	posWS.y = waterHeight;
	float4 posVS = mul(mtxWorldToView, posWS);
	Out.position = Out.posCopy = mul(mtxViewToClip, posVS);
	Out.prevPosition = mul(mtxPrevWorldToClip, posWS);

	return Out;
}

PSOutput mainPS(VSOutput In)
{
	PSOutput Out = (PSOutput)0;

	// ワールド座標を前回のクリップ空間に変換する
	float2 st = In.prevPosition.xy / In.prevPosition.w * float2(0.5, -0.5) + 0.5;
	bool isClipOut = any(st < -1.0) || any(st > 1.0);
	clip(isClipOut ? -1 : 1);

	// 前回フレームのバッファからカラーを取得する
	uint2 frameUV = (uint2)(st * screenInfo.xy);
	Out.color = texPrevReflection[frameUV];

	// ハッシュ情報からブレンド値を変更
	st = In.posCopy.xy / In.posCopy.w * float2(0.5, -0.5) + 0.5;
	frameUV = (uint2)(st * screenInfo.xy);
	uint hash = texProjectHash[frameUV];
	Out.color.a *= !hash ? 1.0 : temporalBlend;

	return Out;
}


#endif // SHADERS_REPROJECT_REFLECTION_HLSLI
//	EOF
