#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD0;
	float3	viewDir		: VIEDIR;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
ConstantBuffer<LightCB>		cbLight			: register(b1);
ConstantBuffer<MaterialCB>	cbMaterial		: register(b2);

Texture2D		texColor	: register(t0);
Texture2D		texNormal	: register(t1);
Texture2D		texORM		: register(t2);
SamplerState	texColor_s	: register(s0);

float4 main(PSInput In) : SV_TARGET0
{
	uint2 pixel = uint2(In.position.xy);

	float4 baseColor = texColor.Sample(texColor_s, In.uv);
	float3 normalInTS = texNormal.Sample(texColor_s, In.uv).xyz * 2 - 1;
	float3 orm = texORM.Sample(texColor_s, In.uv).rgb;
	float roughness = lerp(cbMaterial.roughnessRange.x, cbMaterial.roughnessRange.y, orm.g) + Epsilon;
	float metallic = lerp(cbMaterial.metallicRange.x, cbMaterial.metallicRange.y, orm.b);

	float3 T, B, N;
	GetTangentSpace(In.normal, In.tangent, T, B, N);
	float3 normalInWS = ConvertVectorTangetToWorld(normalInTS, T, B, N);

	float3 diffuseColor = baseColor.rgb * (1 - metallic);
	float3 specularColor = 0.04 * (1 - metallic) + baseColor.rgb * metallic;

	float3 directColor = BrdfGGX(diffuseColor, specularColor, roughness, normalInWS, -cbLight.lightDir.rgb, normalize(-In.viewDir)) * cbLight.lightColor.rgb;
	float3 skyColor = SkyColor(normalInWS) * cbLight.skyPower * diffuseColor;
	float3 finalColor = directColor + skyColor;
	return float4(pow(saturate(finalColor), 1 / 2.2), 1);
}
