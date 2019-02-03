struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD0;
};

Texture2D		texColor	: register(t0);
SamplerState	texColor_s	: register(s0);

float4 main(PSInput In) : SV_TARGET0
{
	return texColor.SampleLevel(texColor_s, In.uv, 0.0);
}
