#ifndef RTXGI_COMMON_DEFINES_HLSLI
#define RTXGI_COMMON_DEFINES_HLSLI

#include "rtxgi_common_defines.h"

#define HLSL

// register settings.
#define CONSTS_REGISTER					b0
#define CONSTS_SPACE					space1
#define VOLUME_CONSTS_REGISTER			t0
#define VOLUME_CONSTS_SPACE				space1
#define RAY_DATA_REGISTER				u0
#define RAY_DATA_SPACE					space1
#define OUTPUT_SPACE					space1
#define PROBE_DATA_REGISTER				u3
#define PROBE_DATA_SPACE				space1

#endif // RTXGI_COMMON_DEFINES_HLSLI
