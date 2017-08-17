struct VSInput
{
	float4	position	: POSITION;
	float4	color		: COLOR;
	float2	uv			: TEXCOORD0;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float4	color		: COLOR;
	float2	uv			: TEXCOORD0;
};

cbuffer CBScene : register(b0)
{
	float4x4	mtxW_;
	float4x4	mtxV_;
	float4x4	mtxP_;
};

VSOutput main(VSInput In)
{
	VSOutput Out;

	Out.position = mul(mtxP_, mul(mtxV_, mul(mtxW_, In.position)));
	Out.color = In.color;
	Out.uv = In.uv;

	return Out;
}

//	EOF
