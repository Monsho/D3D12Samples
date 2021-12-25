#include "constant.h"
#include "colorspace.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<UIDrawCB>	cbUI	: register(b0);

Texture2D		texUI		: register(t0);
SamplerState	samLinear	: register(s0);

float4 main(VSOutput In) : SV_Target0
{
	float4 color = texUI.SampleLevel(samLinear, In.uv, 0.0);

	color.rgb *= cbUI.intensity;
	if (cbUI.colorSpace > 0)
	{
		color.rgb = Rec709ToRec2020(color.rgb);
		if (cbUI.colorSpace == 2)
			color.rgb = LinearToST2084(color.rgb);
	}

	float alpha = color.a * cbUI.alpha;
	color.rgb *= alpha;
	color.a = cbUI.blendType == 0 ? (1.0 - alpha) : 1.0;

	return color;
}

float4 mainIndirect(VSOutput In) : SV_Target0
{
	float4 color = texUI.SampleLevel(samLinear, In.uv, 0.0);

	color.rgb *= cbUI.intensity;
	if (cbUI.colorSpace > 0)
	{
		color.rgb = Rec709ToRec2020(color.rgb);
	}

	color.rgb *= cbUI.alpha;
	color.a = lerp(1.0, color.a, cbUI.alpha);

	return color;
}

//	EOF
