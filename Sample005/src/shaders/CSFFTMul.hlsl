RWTexture2D<float4>	dstImageR : register(u0);
RWTexture2D<float4>	dstImageI : register(u1);
Texture2D<float4>	srcImageR : register(t0);
Texture2D<float4>	srcImageI : register(t1);
Texture2D<float4>	filterImageR : register(t2);
Texture2D<float4>	filterImageI : register(t3);


[numthreads(32, 32, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint2 position = dispatchID.xy;

	float4 R1 = srcImageR[position];
	float4 I1 = srcImageI[position];
	float4 R2 = filterImageR[(position + 256) % 512];
	//float4 R2 = filterImageR[position];
	float4 I2 = filterImageI[position];

	float2 p = ((float2)position + 0.5) / 512;
	p = frac(p + 0.5);
	float2 d = p - 0.5;
	d.x *= 8.0;
	float l = sqrt(d.x * d.x + d.y * d.y);
	float3 R = R1.rgb;
	float3 I = I1.rgb;
	float t = saturate(smoothstep(1.0, 0.0, l / 0.1));
	R *= R2.rgb;
	I *= R2.rgb;

	//float3 R = R1.rgb * R2.rgb - I1.rgb * I2.rgb;
	//float3 I = R1.rgb * I2.rgb + I1.rgb * R2.rgb;

	dstImageR[position] = float4(R, R1.a);
	dstImageI[position] = float4(I, I1.a);
}
