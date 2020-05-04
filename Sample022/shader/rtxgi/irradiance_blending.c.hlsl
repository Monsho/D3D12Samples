#include "volume_desc.h"

#define RTXGI_DDGI_BLEND_RADIANCE			1
#define RAYS_PER_PROBE						SGI_NUM_RAYS_PER_PROBE
#define PROBE_NUM_TEXELS					SGI_NUM_IRRADIANCE_TEXELS
#define PROBE_UAV_INDEX						0
#include "ddgi/ProbeBlendingCS.hlsl"

//	EOF
