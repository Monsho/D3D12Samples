struct PSInput
{
	float4	pos		: SV_POSITION;
	float2	uv		: TEXCOORD0;
};

Texture2D		texBase;
SamplerState	texBase_s;

float4 main(PSInput In)	: SV_TARGET0
{
	float4 c = texBase.SampleLevel(texBase_s, In.uv, 0);

	return c;
}

//	EOF
