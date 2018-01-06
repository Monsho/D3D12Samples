#ifndef SHADERS_BLUR_HLSLI
#define SHADERS_BLUR_HLSLI

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

cbuffer CbGaussBlur
{
	float4	gaussWeights;
	float2	deltaUV;
};

Texture2D		texSource;
Texture2D		texLinearDepth;
SamplerState	samLinearClamp;

float4 mainPS(VSOutput In)
{
	float4 center = texSource.SampleLevel(samLinearClamp, In.uv, 0);
	float3 c = center.rgb * gaussWeights.x;

	float linearDepth = texLinearDepth.SampleLevel(samLinearClamp, In.uv, 0).r;

#if BLUR_VERTICAL == 0
	float2 delta = deltaUV * float2(1, 0);
#else
	float2 delta = deltaUV * float2(0, 1);
#endif
	delta *= linearDepth * 5.0f;		// âúÇÃï˚Ç™ÉuÉâÅ[Ç™Ç©Ç©ÇËÇ‚Ç∑Ç¢ÇÊÇ§Ç…

	float2 d = delta;
	c += texSource.SampleLevel(samLinearClamp, In.uv + d, 0).rgb * gaussWeights.y;
	c += texSource.SampleLevel(samLinearClamp, In.uv - d, 0).rgb * gaussWeights.y;

	d += delta;
	c += texSource.SampleLevel(samLinearClamp, In.uv + d, 0).rgb * gaussWeights.z;
	c += texSource.SampleLevel(samLinearClamp, In.uv - d, 0).rgb * gaussWeights.z;

	d += delta;
	c += texSource.SampleLevel(samLinearClamp, In.uv + d, 0).rgb * gaussWeights.w;
	c += texSource.SampleLevel(samLinearClamp, In.uv - d, 0).rgb * gaussWeights.w;

	return float4(c, center.a);
}

#endif // SHADERS_BLUR_HLSLI
//	EOF
