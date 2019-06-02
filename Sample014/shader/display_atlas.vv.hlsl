#include "common.hlsli"

struct VSInput
{
	float2	uv			: TEXCOORD;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
};

VSOutput main(const VSInput In)
{
	VSOutput Out;

	Out.position = float4(In.uv * float2(2, -2) + float2(-1, 1), 0, 1);

	return Out;
}

//	EOF
