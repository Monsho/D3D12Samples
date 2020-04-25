#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD;

	float4	currPosCS	: CURR_POS_CS;
	float4	prevPosCS	: PREV_POS_CS;
};

struct PSOutput
{
	float4	normal			: SV_TARGET0;
	float4	baseColor		: SV_TARGET1;
	float4	motion_rm		: SV_TARGET2;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
ConstantBuffer<MaterialCB>	cbMaterial		: register(b1);

Texture2D		texColor		: register(t0);
Texture2D		texNormal		: register(t1);
Texture2D		texORM			: register(t2);
SamplerState	texColor_s		: register(s0);

PSOutput main(PSInput In)
{
	PSOutput Out;

	float4 baseColor = texColor.Sample(texColor_s, In.uv);
	clip(baseColor.a < 0.33 ? -1.0 : 1.0);

	float3 normalInTS = texNormal.Sample(texColor_s, In.uv).xyz * 2 - 1;
	float3 orm = texORM.Sample(texColor_s, In.uv).rgb;
	float roughness = lerp(cbMaterial.roughnessRange.x, cbMaterial.roughnessRange.y, orm.g);
	float metallic = lerp(cbMaterial.metallicRange.x, cbMaterial.metallicRange.y, orm.b);

	float3 T, B, N;
	GetTangentSpace(In.normal, In.tangent, T, B, N);
	float3 normalInWS = ConvertVectorTangetToWorld(normalInTS, T, B, N);
	Out.normal = float4(normalInWS * 0.5 + 0.5, 1);
	Out.baseColor = baseColor;

	float2 cuv = (In.currPosCS.xy / In.currPosCS.w) * float2(0.5, -0.5) + 0.5;
	float2 puv = (In.prevPosCS.xy / In.prevPosCS.w) * float2(0.5, -0.5) + 0.5;
	Out.motion_rm = float4(cuv - puv, roughness, metallic);

	return Out;
}
