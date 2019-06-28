struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD;
};

Texture2D		texImage		: register(t0);
SamplerState	texImage_s		: register(s0);

float4 main(PSInput In) : SV_TARGET0
{
	float a = texImage.Sample(texImage_s, In.uv).a;
	clip(a < 1.0 ? -1 : 1);

	float3 normal = normalize(In.normal);
	return float4(normal * 0.5 + 0.5, 1);
}
