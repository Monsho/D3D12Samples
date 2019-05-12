struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

Texture2D		texImage		: register(t0);
SamplerState	texImage_s		: register(s0);

void main(PSInput In)
{
	float opacity = texImage.Sample(texImage_s, In.uv).a;
	clip(opacity < 0.33 ? -1 : 1);
}
