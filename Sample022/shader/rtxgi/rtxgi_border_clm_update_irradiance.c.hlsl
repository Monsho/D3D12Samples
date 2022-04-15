#include "volume_desc.h"
#include "rtxgi_common_defines.hlsli"

#define RTXGI_DDGI_BLEND_RADIANCE		1
#define RTXGI_DDGI_PROBE_NUM_TEXELS		SGI_NUM_IRRADIANCE_TEXELS
#define OUTPUT_REGISTER					u1
#include "ddgi/ProbeBorderUpdateCS.hlsl"

// EOF
