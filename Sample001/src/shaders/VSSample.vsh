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
	float4x4	mtxW_;
	float4x4	mtxV_;
	float4x4	mtxP_;
};

VSOutput main(VSInput In)
{
	VSOutput Out;

	Out.position = mul(mtxP_, mul(mtxV_, mul(mtxW_, In.position)));
	//Out.position = mul(mul(mul(In.position, mtxW_), mtxV_), mtxP_);
	Out.color = In.color;

	return Out;
}

//	EOF
