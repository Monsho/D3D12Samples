#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD0;
};

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);
ConstantBuffer<GlobalIlluminationCB>	cbGI			: register(b2);
ConstantBuffer<ReflectionCB>			cbReflection	: register(b3);

Texture2D			texNormal		: register(t0);
Texture2D			texBaseColor	: register(t1);
Texture2D			texMotionRM		: register(t2);
Texture2D<float>	texDepth		: register(t3);
Texture2D<float>	texScreenShadow	: register(t4);
Texture2D<float3>	texGI			: register(t5);
Texture2D			texReflection	: register(t6);
Texture2D			texSkyHdr		: register(t7);
SamplerState		texColor_s		: register(s0);
SamplerState		texSkyHdr_s		: register(s1);

float4 main(PSInput In) : SV_TARGET0
{
	// from depth to world position.
	uint2 pixel = uint2(In.position.xy);
	float depth = texDepth[pixel];
	if (depth >= 1.0)
	{
		return float4(0, 0, 1, 1);
	}
	float2 clipSpacePos = In.uv * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	float4 baseColor = texBaseColor[pixel];
	float3 normalInWS = texNormal[pixel].xyz * 2.0 - 1.0;
	float2 rm = texMotionRM[pixel].zw;
	float direct_shadow = texScreenShadow[pixel];

	float roughness = rm.x + Epsilon;
	float metallic = rm.y;

	float3 diffuseColor = baseColor.rgb * (1 - metallic);
	float3 specularColor = 0.04 * (1 - metallic) + baseColor.rgb * metallic;

	float3 V = normalize(cbScene.camPos.xyz - worldPos.xyz);
	float3 directColor = BrdfGGX(diffuseColor, specularColor, roughness, normalInWS, -cbLight.lightDir.rgb, V) * cbLight.lightColor.rgb;
	float3 giColor = texGI[pixel] * cbGI.intensity * diffuseColor;
	float3 reflectionColor = SpecularFSchlick(saturate(dot(V, normalInWS)), baseColor.rgb * metallic)
		* texReflection.SampleLevel(texColor_s, In.uv, 0).rgb * cbReflection.intensity;
	//float3 reflectionColor = (1 - pow(1 - saturate(dot(V, normalInWS)), 10.0)) * baseColor.rgb * metallic
	//	* texReflection.SampleLevel(texColor_s, In.uv, 0).rgb * cbReflection.intensity;
	float3 skyColor = SkyTextureLookup(texSkyHdr, texSkyHdr_s, normalInWS) * cbLight.skyPower * diffuseColor;
	//float3 finalColor = directColor * direct_shadow + giColor + reflectionColor;
	float3 finalColor = directColor * direct_shadow + skyColor + reflectionColor;
	return float4(pow(saturate(finalColor), 1 / 2.2), 1);
}
