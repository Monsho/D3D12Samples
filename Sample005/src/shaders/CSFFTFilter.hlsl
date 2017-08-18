cbuffer CbFilter
{
	float2	distance_scale;
	float	radius;
	float	inverse;
};

RWTexture2D<float4>	dstImageR : register(u0);
RWTexture2D<float4>	dstImageI : register(u1);
Texture2D<float4>	srcImageR : register(t0);
Texture2D<float4>	srcImageI : register(t1);


[numthreads(32, 32, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint2 position = dispatchID.xy;

	float4 R_in = srcImageR[position];
	float4 I_in = srcImageI[position];

	float2 p = ((float2)position + 0.5) / 512;
	p = frac(p + 0.5);
	float2 d = p - 0.5;
	d *= distance_scale;
	float l = sqrt(d.x * d.x + d.y * d.y);
	float t = saturate(smoothstep(1.0 - inverse, inverse, l / radius));
	float3 R = R_in.rgb * t;
	float3 I = I_in.rgb * t;

	dstImageR[position] = float4(R, R_in.a);
	dstImageI[position] = float4(I, I_in.a);
}
