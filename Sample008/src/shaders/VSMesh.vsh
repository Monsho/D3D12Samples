struct VSInput
{
	float4	position	: POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD0;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	normalWS	: NORMAL;
	float2	uv			: TEXCOORD0;
};

cbuffer CBScene : register(b0)
{
	float4x4	mtxWorldToView;
	float4x4	mtxViewToClip;
	float4x4	mtxViewToWorld;
	float4		screenInfo;
	float4		frustumCorner;
};

VSOutput main(VSInput In)
{
	VSOutput Out;

	Out.position = mul(mtxViewToClip, mul(mtxWorldToView, In.position));
	Out.normalWS = normalize(In.normal);
	Out.uv = In.uv;

	return Out;
}

//	EOF
