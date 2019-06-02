#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD0;
	float2	unique_uv	: TEXCOORD1;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);

Texture2D		texColor		: register(t0);
SamplerState	texColor_s		: register(s0);

Texture2D		texOcclude		: register(t1);

Texture2D		texLightMap		: register(t2);
SamplerState	texLightMap_s	: register(s2);

float4 main(PSInput In) : SV_TARGET0
{
	uint2 pixel = uint2(In.position.xy);
	float4 occlude = texOcclude[pixel];

	float4 baseColor = texColor.Sample(texColor_s, In.uv);
	clip(baseColor.a < 0.33 ? -1 : 1);

	float3 normal = normalize(In.normal);
	float LoN = saturate(dot(normal, -cbScene.lightDir.rgb));
	float3 directColor = cbScene.lightColor.rgb * LoN * occlude.r;
	float3 indirectColor = texLightMap.Sample(texLightMap_s, In.unique_uv).rgb;
	//return float4(indirectColor, 1);
	return float4((directColor + indirectColor) * baseColor.rgb, 1);
}
