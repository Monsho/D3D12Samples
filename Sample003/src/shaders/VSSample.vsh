struct VSInput
{
	float4	position	: POSITION;
	float4	color		: COLOR;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float4	color		: COLOR;
};

cbuffer CBScene : register(b0)
{
	float4x4	mtxVP_;
};

VSOutput main(VSInput In)
{
	VSOutput Out;

	Out.position = mul(mtxVP_, In.position);
	Out.color = In.color;

	return Out;
}

//	EOF
