#define BLUR_VERTICAL		1
#include "blur.hlsli"

float4 main(VSOutput In) : SV_TARGET0
{
	return mainPS(In);
}

//	EOF
