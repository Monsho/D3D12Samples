#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD0;
	float3	viewDir		: VIEDIR;
	float4	color		: COLOR;
};

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);
ConstantBuffer<GlobalIlluminationCB>	cbGI			: register(b2);
ConstantBuffer<MaterialCB>				cbMaterial		: register(b3);

Texture2D			texColor		: register(t0);
Texture2D			texNormal		: register(t1);
Texture2D			texORM			: register(t2);
Texture2D<float>	texScreenShadow	: register(t3);
Texture2D<float3>	texGI			: register(t4);
SamplerState		texColor_s		: register(s0);

float4 main(PSInput In) : SV_TARGET0
{
	float4 mult_culor = (1.0).xxxx;
	if (cbScene.isMeshletColor)
	{
		mult_culor = In.color;
	}

	uint2 pixel = uint2(In.position.xy);
	float direct_shadow = texScreenShadow[pixel];

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
	float3 giColor = texGI[pixel] * cbGI.intensity * diffuseColor;
	//float3 skyColor = SkyColor(normalInWS) * cbLight.skyPower * diffuseColor;
	float3 finalColor = directColor * direct_shadow + giColor;
	return float4(pow(saturate(finalColor), 1 / 2.2), 1) * mult_culor;
}
