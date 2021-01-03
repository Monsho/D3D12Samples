#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD0;
	float4	color		: COLOR;
};

struct PSOutput
{
	float4	worldQuat	: SV_TARGET0;
	float4	baseColor	: SV_TARGET1;
	float4	roughMetal	: SV_TARGET2;
	float4	multColor	: SV_TARGET3;
};

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<MaterialCB>				cbMaterial		: register(b1);

Texture2D			texColor		: register(t0);
Texture2D			texNormal		: register(t1);
Texture2D			texORM			: register(t2);
SamplerState		texColor_s		: register(s0);

PSOutput main(PSInput In)
{
	PSOutput Out = (PSOutput)0;
	Out.multColor = (1.0).xxxx;
	if (cbScene.isMeshletColor)
	{
		Out.multColor = In.color;
	}

	GBuffer gb;
	gb.baseColor = texColor.Sample(texColor_s, In.uv);
	float3 orm = texORM.Sample(texColor_s, In.uv).rgb;
	gb.roughness = lerp(cbMaterial.roughnessRange.x, cbMaterial.roughnessRange.y, orm.g) + Epsilon;
	gb.metallic = lerp(cbMaterial.metallicRange.x, cbMaterial.metallicRange.y, orm.b);

#if 0
	float3 T, B, N;
	GetTangentSpace(In.normal, In.tangent, T, B, N);
	float3 normalInTS = texNormal.Sample(texColor_s, In.uv).xyz * 2 - 1;
	float3 normalInWS = ConvertVectorTangetToWorld(normalInTS, T, B, N);
	B = normalize(cross(normalInWS, T));
	T = cross(B, normalInWS);
	gb.worldQuat = TangentSpaceToQuat(T, B, normalInWS);
#else
	float3 T, B, N;
	GetTangentSpace(In.normal, In.tangent, T, B, N);
	float4 worldQuat = TangentSpaceToQuat(T, B * -sign(In.tangent.w), N);
	float3 normalInTS = texNormal.Sample(texColor_s, In.uv).xyz * 2 - 1;
	normalInTS *= float3(1, -sign(In.tangent.w), 1);
	float3 normalInWS = normalize(ConvertVectorTangentToWorld(normalInTS, worldQuat));
	B = normalize(cross(normalInWS, T));
	T = cross(B, normalInWS);
	gb.worldQuat = TangentSpaceToQuat(T, B, normalInWS);
#endif

	EncodeGBuffer(gb, Out.worldQuat, Out.baseColor, Out.roughMetal);

	return Out;
}
