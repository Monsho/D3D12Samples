#ifndef TONEMAP_HLSLI
#define TONEMAP_HLSLI

float3 ReinhardTonemap(float3 color, float baseLum, float maxLum)
{
	float I = maxLum / baseLum;
	float k = baseLum * I / (baseLum - I);
	return color * k / (color + k);
}

float3 GTTonemap(float3 color, float baseLum, float maxLum)
{
	float k = maxLum / baseLum;

	float P = k;
	float a = 1.0;
	float m = 0.22;
	float l = 0.4;
	float c = 1.33;
	float b = 0.0;

	float3 x = color.rgb;
	float l0 = ((P - m) * l) / a;
	float L0 = m - (m / a);
	float L1 = m + ((1.0 - m) / a);

	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = (a * P) / (P - S1);
	float CP = -C2 / P;

	float3 w0 = 1.0 - smoothstep(0.0, m, x);
	float3 w2 = step(m + l0, x);
	float3 w1 = 1.0 - w0 - w2;

	float3 T = m * pow(x / m, c) + b;
	float3 S = P - (P - S1) * exp(CP * (x - S0));
	float3 L = m + a * (x - m);

	return T * w0 + L * w1 + S * w2;
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilmicTonemap(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

#endif // TONEMAP_HLSLI
//	EOF
