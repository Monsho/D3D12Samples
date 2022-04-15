#include "common.hlsli"
#include "colorspace.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
cbuffer cbReflectionDisplay : register(b1)
{
	uint	DisplayType_;
}

Texture2D					texGBuffer0			: register(t0);
Texture2D					texGBuffer1			: register(t1);
Texture2D					texGBuffer2			: register(t2);
Texture2D<float>			texDepth			: register(t3);
Texture2D					texReflection		: register(t4);

SamplerState				samLinearClamp		: register(s0);

float4 main(VSOutput In) : SV_Target0
{
	uint2 pixelPos = uint2(In.position.xy);
	float depth = texDepth[pixelPos];
	clip(depth >= 1.0 ? -1 : 1);

	// get gbuffer.
	float4 gin0 = texGBuffer0[pixelPos];
	float4 gin1 = texGBuffer1[pixelPos];
	float4 gin2 = texGBuffer2[pixelPos];
	GBuffer gb = DecodeGBuffer(gin0, gin1, gin2);

	// get world position.
	float2 screenPos = ((float2)pixelPos + 0.5) / cbScene.screenInfo.zw;
	float2 clipSpacePos = screenPos * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	float3 viewDirInWS = normalize(worldPos.xyz - cbScene.camPos.xyz);
	float3 normalInWS = ConvertVectorTangentToWorld(float3(0, 0, 1), gb.worldQuat);
	float3 specularColor = 0.04 * (1 - gb.metallic) + gb.baseColor.rgb * gb.metallic;
	if (cbScene.renderColorSpace != 0)
	{
		specularColor = Rec709ToRec2020(specularColor);
	}

	float3 radiance = texReflection[pixelPos];
	float NoV = saturate(dot(normalInWS, -viewDirInWS));
	float3 result = SpecularFSchlick(NoV, specularColor) * radiance;

	return float4(result, DisplayType_ == 0 ? 1 : 0);
}

//	EOF
