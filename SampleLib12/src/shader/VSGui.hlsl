cbuffer cbScene : register(b0)
{
	float2	uScale;
	float2	uTranslate;
};

struct VS_INPUT
{
	float2	pos		: POSITION;
	float2	uv		: TEXCOORD;
	float4	color	: COLOR;
};

struct VS_OUTPUT
{
	float4	pos		: SV_POSITION;
	float2	uv		: TEXCOORD;
	float4	color	: COLOR;
};

VS_OUTPUT main(VS_INPUT In)
{
	VS_OUTPUT Out;
	Out.pos = float4(In.pos * uScale + uTranslate, 0.0, 1.0);
	Out.color = In.color;
	Out.uv = In.uv;
	return Out;
}
