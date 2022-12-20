#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD0;
	float4	currPos		: CURR_POS;
	float4	prevPos		: PREV_POS;
};

struct PSOutput
{
	float3	emission	: SV_TARGET0;
	float4	worldQuat	: SV_TARGET1;
	float4	baseColor	: SV_TARGET2;
	float4	roughMetal	: SV_TARGET3;
	float2	velocity	: SV_TARGET4;
};

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<MaterialCB>				cbMaterial		: register(b1);
ConstantBuffer<MeshMaterialCB>			cbMeshMaterial	: register(b2);

Texture2D			texColor		: register(t0);
Texture2D			texNormal		: register(t1);
Texture2D			texORM			: register(t2);
SamplerState		texColor_s		: register(s0);

PSOutput main(PSInput In)
{
	PSOutput Out = (PSOutput)0;

	GBuffer gb;
	gb.baseColor = texColor.Sample(texColor_s, In.uv) * cbMeshMaterial.baseColor;
	float3 orm = texORM.Sample(texColor_s, In.uv).rgb;
	gb.roughness = lerp(cbMaterial.roughnessRange.x, cbMaterial.roughnessRange.y, orm.g * cbMeshMaterial.roughness) + Epsilon;
	gb.metallic = lerp(cbMaterial.metallicRange.x, cbMaterial.metallicRange.y, orm.b * cbMeshMaterial.metallic);

	// TEST
	//float2 uv_in_size = In.uv * 4096.0;
	//float2 derivX = ddx(uv_in_size);
	//float2 derivY = ddy(uv_in_size);
	//float delta_max_sqr = max(dot(derivX, derivX), dot(derivY, derivY));
	//float mip = max(0, 0.5 * log2(delta_max_sqr));
	//const float3 kMipColors[12] = {
	//	float3(1, 0, 0),
	//	float3(1, 1, 0),
	//	float3(0, 1, 0),
	//	float3(0, 1, 1),
	//	float3(0, 0, 1),
	//	float3(0.25, 0, 1),
	//	float3(0.25, 0, 0),
	//	float3(0.25, 0.25, 0),
	//	float3(0, 0.25, 0),
	//	float3(0, 0.25, 0.25),
	//	float3(0, 0, 0.25),
	//	float3(0, 0, 0),
	//};
	//gb.baseColor.rgb = kMipColors[int(mip)];
	// TEST

	float3 T, B, N;
	GetTangentSpace(In.normal, In.tangent, T, B, N);
	float4 worldQuat = TangentSpaceToQuat(T, B * -sign(In.tangent.w), N);
	float3 normalInTS = texNormal.Sample(texColor_s, In.uv).xyz * 2 - 1;
	normalInTS *= float3(1, -sign(In.tangent.w), 1);
	float3 normalInWS = normalize(ConvertVectorTangentToWorld(normalInTS, worldQuat));
	B = normalize(cross(normalInWS, T));
	T = cross(B, normalInWS);
	gb.worldQuat = TangentSpaceToQuat(T, B, normalInWS);

	EncodeGBuffer(gb, Out.worldQuat, Out.baseColor, Out.roughMetal);

	// output screen velocity.
	float2 cp = In.currPos.xy / In.currPos.w;
	float2 pp = In.prevPos.xy / In.prevPos.w;
	Out.velocity = cp - pp;

	// emission
	Out.emission = cbMeshMaterial.emissiveColor * cbMaterial.emissiveIntensity;

	return Out;
}
