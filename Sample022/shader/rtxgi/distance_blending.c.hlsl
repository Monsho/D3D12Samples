#include "volume_desc.h"

#define RTXGI_DDGI_BLEND_RADIANCE			0
#define RAYS_PER_PROBE						SGI_NUM_RAYS_PER_PROBE
#define PROBE_NUM_TEXELS					SGI_NUM_DISTANCE_TEXELS
#define PROBE_UAV_INDEX						1
#include "ddgi/ProbeBlendingCS.hlsl"

//	EOF
