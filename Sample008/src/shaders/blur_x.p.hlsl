#define BLUR_VERTICAL		0
#include "blur.hlsli"

float4 main(VSOutput In) : SV_TARGET0
{
	return mainPS(In);
}

//	EOF
