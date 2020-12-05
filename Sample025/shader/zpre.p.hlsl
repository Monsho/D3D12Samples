#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);

Texture2D		texImage		: register(t0);
SamplerState	texImage_s		: register(s0);

void main(PSInput In)
{
	float a = texImage.Sample(texImage_s, In.uv).a;
	clip(a < 0.5 ? -1 : 1);
}
