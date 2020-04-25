#include "raytrace.hlsli"

Texture2D<float4>	texSkyHdr	: register(t0);
SamplerState		texSkyHdr_s	: register(s0);
RWTexture2D<float4>	rwIrr		: register(u0);

#define N		128

[numthreads(8, 8, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint2 dst_pos = dispatchID.xy;
	uint w, h;
	rwIrr.GetDimensions(w, h);
	if (dst_pos.x >= w || dst_pos.y >= h)
		return;

	uint srcw, srch;
	texSkyHdr.GetDimensions(srcw, srch);
	float texel_angle = 4 * PI / float(srcw * srch);

	// uv to sphere vector.
	float2 uv = ((float2)dst_pos + 0.5) / float2(w, h);
	float2 rad = uv * PI;
	float3 n = {
		cos(rad.x * 2 - PI)* sin(rad.y),
		cos(rad.y),
		sin(rad.x * 2 - PI)* sin(rad.y)
	};

	// get onb.
	float3x3 onb = CalcONB(n);

	// importance sampling.
	float3 total = 0;
	for (int i = 0; i < N; i++)
	{
		float2 ham = Hammersley2D(i, N);
		float3 d = HemisphereSampleCos(ham);
		d = mul(d, onb);

		float pdf = dot(d, n) / PI;
		float sample_angle = 1.0 / (N * pdf);
		float lod = 0.5 * log2(sample_angle / texel_angle);

		total += SkyTextureLookup(texSkyHdr, texSkyHdr_s, d, lod);
	}
	rwIrr[dst_pos] = float4(total / float(N), 1);
}

//	EOF
