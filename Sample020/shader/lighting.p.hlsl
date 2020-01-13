#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
ConstantBuffer<LightCB>		cbLight			: register(b1);

Texture2D			texNormalWS		: register(t0);
Texture2D			texBaseColor	: register(t1);
Texture2D			texORM			: register(t2);
Texture2D<float>	texDepth		: register(t3);
SamplerState		texColor_s		: register(s0);

float4 main(PSInput In) : SV_TARGET0
{
	float depth = texDepth.SampleLevel(texColor_s, In.uv, 0.0);
	if (depth >= 1.0)
	{
		return float4(0, 0, 1, 1);
	}

	float4 posInCS = float4(In.uv * float2(2, -2) + float2(-1, 1), depth, 1);
	float4 posInWS = mul(cbScene.mtxProjToWorld, posInCS);
	posInWS.xyz /= posInWS.w;

	float4 baseColor = texBaseColor.SampleLevel(texColor_s, In.uv, 0.0);
	float3 normalInWS = texNormalWS.SampleLevel(texColor_s, In.uv, 0.0).xyz * 2 - 1;
	float3 orm = texORM.SampleLevel(texColor_s, In.uv, 0.0).rgb;
	float roughness = orm.g;
	float metallic = orm.b;

	float3 diffuseColor = baseColor.rgb * (1 - metallic);
	float3 specularColor = 0.04 * (1 - metallic) + baseColor.rgb * metallic;
	float3 viewDir = normalize(posInWS.xyz - cbScene.camPos.xyz);

	float3 directColor = BrdfGGX(diffuseColor, specularColor, roughness, normalInWS, -cbLight.lightDir.rgb, -viewDir) * cbLight.lightColor.rgb;
	float3 skyColor = SkyColor(normalInWS) * cbLight.skyPower * diffuseColor;
	float3 finalColor = directColor + skyColor;
	return float4(pow(saturate(finalColor), 1 / 2.2), 1);
}
