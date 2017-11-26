#include "const_buffer.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

Texture2D	texGBuffer0;
Texture2D	texGBuffer1;
Texture2D	texGBuffer2;
Texture2D	texLinearDepth;

float4 main(VSOutput In) : SV_TARGET0
{
	uint2 frameUV = (uint2)(In.uv * screenInfo.xy);

	// テクスチャサンプリング
	float3 normalWS = normalize(texGBuffer0[frameUV].xyz);
	float3 baseColor = texGBuffer1[frameUV].rgb;
	float2 metalRough = texGBuffer2[frameUV].rg;
	float  linearDepth = texLinearDepth[frameUV].r;

	// 座標を求める
	float2 uv = In.position.xy / In.position.w;
	float3 frustumVec = { frustumCorner.x * uv.x, frustumCorner.y * uv.y, -frustumCorner.z };
	float3 posVS = frustumVec * linearDepth;
	float3 posWS = mul(mtxViewToWorld, float4(posVS, 1)).xyz;

	// 簡易ライティング
	return float4(baseColor * normalWS.y, 1);
}


//	EOF
