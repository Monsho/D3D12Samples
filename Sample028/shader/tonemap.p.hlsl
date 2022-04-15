#include "constant.h"
#include "colorspace.hlsli"
#include "tonemap.hlsli"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<TonemapCB>	cbTonemap	: register(b0);

Texture2D		texInput	: register(t0);

float4 TonemapRec709(VSOutput In) : SV_Target0
{
	float4 color = texInput[In.position.xy];

	if (cbTonemap.renderColorSpace != 0)
	{
		color.rgb = Rec2020ToRec709(color.rgb);
	}

	if (cbTonemap.type == 1)
	{
		// Reinhard tonemap.
		color.rgb = ReinhardTonemap(color.rgb, cbTonemap.baseLuminance, cbTonemap.maxLuminance);
	}
	else if (cbTonemap.type == 2)
	{
		// to Rec2020
		color.rgb = Rec709ToRec2020(color.rgb);

		// GT tonemap.
		color.rgb = GTTonemap(color.rgb, cbTonemap.baseLuminance, cbTonemap.maxLuminance);

		// to Rec709
		color.rgb = Rec2020ToRec709(color.rgb);
	}
	else if (cbTonemap.type == 3)
	{
		// Test green gradiation.
		color.rgb = float3(0.0f, In.uv.x, 0.0f);
		if (In.uv.y < 0.5)
		{
			color.rgb = Rec2020ToRec709(color.rgb);
		}
	}

	// to sRGB.
	color.rgb = LinearToSRGB(color.rgb);

	return color;
}

float4 TonemapRec2020(VSOutput In) : SV_Target0
{
	float4 color = texInput[In.position.xy];

	// to Rec2020
	if (cbTonemap.renderColorSpace == 0)
	{
		color.rgb = Rec709ToRec2020(color.rgb);
	}

	if (cbTonemap.type == 1)
	{
		// Reinhard tonemap.
		color.rgb = ReinhardTonemap(color.rgb, cbTonemap.baseLuminance, cbTonemap.maxLuminance);
	}
	else if (cbTonemap.type == 2)
	{
		// GT tonemap.
		color.rgb = GTTonemap(color.rgb, cbTonemap.baseLuminance, cbTonemap.maxLuminance);
	}
	else if (cbTonemap.type == 3)
	{
		// Test green gradiation.
		color.rgb = float3(0.0f, In.uv.x, 0.0f);
		if (In.uv.y > 0.5)
		{
			color.rgb = Rec709ToRec2020(color.rgb);
		}
	}

	// to ST2084.
	color.rgb = LinearToST2084(color.rgb);

	return color;
}

float4 OetfSRGB(VSOutput In) : SV_Target0
{
	float4 color = texInput[In.position.xy];

	// to sRGB.
	color.rgb = LinearToSRGB(color.rgb);

	return color;
}

float4 OetfST2084(VSOutput In) : SV_Target0
{
	float4 color = texInput[In.position.xy];

	// to ST2084.
	color.rgb = LinearToST2084(color.rgb);

	return color;
}

float4 EotfSRGB(VSOutput In) : SV_Target0
{
	float4 color = texInput[In.position.xy];

	// to Linear.
	color.rgb = SRGBToLinear(color.rgb);

	return color;
}

float4 EotfST2084(VSOutput In) : SV_Target0
{
	float4 color = texInput[In.position.xy];

	// to Linear.
	color.rgb = ST2084ToLinear(color.rgb);

	return color;
}


//	EOF
