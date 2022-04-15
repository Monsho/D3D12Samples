#include "common.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene		: register(b0);

Texture2D			texHDRI		: register(t0);
Texture2D<float>	texDepth	: register(t1);
SamplerState		samHDRI		: register(s0);

float2 CubemapDirToPanoramaUV(float3 dir)
{
	return vec2(
		0.5f + 0.5f * atan(dir.z, dir.x) / PI,
		acos(dir.y) / PI);
}

float4 main(VSOutput In) : SV_Target0
{
	float depth = texDepth[In.position.xy];
	clip(depth < 1.0 ? -1 : 1);

	float2 clipSpacePos = In.uv * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;
	float3 viewDirInWS = normalize(worldPos.xyz - cbScene.camPos.xyz);

	float2 panoramaUV = CubemapDirToPanoramaUV(viewDirInWS);
	return float4(texHDRI.SampleLevel(samHDRI, panoramaUV, 0).rgb, 1);
}

//	EOF
