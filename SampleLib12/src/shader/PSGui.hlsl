struct VS_OUTPUT
{
	float4	pos		: SV_POSITION;
	float2	uv		: TEXCOORD;
	float4	color	: COLOR;
};

sampler		samFont		: register(s0);
Texture2D	texFont		: register(t0);

float4 main(VS_OUTPUT In) : SV_TARGET
{
	float4 Out = In.color * texFont.Sample(samFont, In.uv);
	return Out;
}
