#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD;

	float4	currPosCS	: CURR_POS_CS;
	float4	prevPosCS	: PREV_POS_CS;
};

struct PSOutput
{
	float4	normal			: SV_TARGET0;
	float4	linearDepth		: SV_TARGET1;
	float4	motion			: SV_TARGET2;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);

Texture2D		texImage		: register(t0);
SamplerState	texImage_s		: register(s0);

PSOutput main(PSInput In)
{
	PSOutput Out;

	float a = texImage.Sample(texImage_s, In.uv).a;
	clip(a < 1.0 ? -1 : 1);

	float3 normal = normalize(In.normal);
	float dN = length(fwidth(normal));
	Out.normal = float4(normal * 0.5 + 0.5, 1);

	float linearZ = In.currPosCS.z / cbScene.screenInfo.y;
	float maxDeltaZ = max(abs(ddx(linearZ)), abs(ddy(linearZ)));
	Out.linearDepth = float4(linearZ, maxDeltaZ, In.prevPosCS.z / cbScene.screenInfo.y, dN);

	float2 cuv = (In.currPosCS.xy / In.currPosCS.w) * float2(0.5, -0.5) + 0.5;
	float2 puv = (In.prevPosCS.xy / In.prevPosCS.w) * float2(0.5, -0.5) + 0.5;
	Out.motion = float4(cuv - puv, 0, 0);

	return Out;
}
