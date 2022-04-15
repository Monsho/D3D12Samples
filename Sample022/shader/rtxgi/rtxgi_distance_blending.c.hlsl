#include "volume_desc.h"
#include "rtxgi_common_defines.hlsli"

#define RTXGI_DDGI_BLEND_RADIANCE			0
#define RTXGI_DDGI_BLEND_RAYS_PER_PROBE		SGI_NUM_RAYS_PER_PROBE
#define RTXGI_DDGI_PROBE_NUM_TEXELS			SGI_NUM_DISTANCE_TEXELS
#define RTXGI_DDGI_BLEND_SHARED_MEMORY		1
#define OUTPUT_REGISTER						u2
#include "ddgi/ProbeBlendingCS.hlsl"

//	EOF
