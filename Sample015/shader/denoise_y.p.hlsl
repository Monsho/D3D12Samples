#define	kBlurX					0

#include "common.hlsli"
#include "denoise.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

float4 main(PSInput In) : SV_TARGET0
{
	return Denoise(int2(In.position.xy), 3.0);
}
