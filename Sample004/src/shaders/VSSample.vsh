struct VSInput
{
	float4	position	: POSITION;
	float4	normal		: NORMAL;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
};

cbuffer CBScene : register(b0)
{
	float4x4	mtxVP_;
};

VSOutput main(VSInput In)
{
	VSOutput Out;

	Out.position = mul(mtxVP_, In.position);
	Out.normal = In.normal.xyz;

	return Out;
}

//	EOF
