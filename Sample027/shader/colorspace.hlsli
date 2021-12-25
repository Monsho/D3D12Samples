#ifndef COLORSPACE_HLSLI
#define COLORSPACE_HLSLI

float3 LinearToSRGB(float3 lin)
{
	return lin < 0.0031308
		? 12.92 * lin
		: 1.055 * pow(abs(lin), 1.0 / 2.4) - 0.055;
}

float3 SRGBToLinear(float3 srgb)
{
	return srgb < 0.04045
		? srgb / 12.92
		: pow(abs(srgb + 0.055) / 1.055, 2.4);
}

float3 LinearToRec709(float3 lin)
{
	return lin <= 0.018
		? 4.5 * lin
		: 1.099 * pow(abs(lin), 0.45) - 0.099;
}

float3 Rec709ToLinear(float3 bt709)
{
	return bt709 <= 0.081
		? bt709 / 4.5
		: pow(abs(bt709 + 0.099) / 1.099, 1.0 / 0.45);
}

float3 LinearToST2084(float3 lin)
{
	const float m1 = 2610.0 / 4096.0 / 4;
	const float m2 = 2523.0 / 4096.0 * 128;
	const float c1 = 3424.0 / 4096.0;
	const float c2 = 2413.0 / 4096.0 * 32;
	const float c3 = 2392.0 / 4096.0 * 32;
	const float standardNits = 100.0;
	const float maxNits = 10000.0;
	float3 L = lin * standardNits / maxNits;
	float3 cp = pow(abs(L), m1);
	return pow((c1 + c2 * cp) / (1 + c3 * cp), m2);
}

float3 ST2084ToLinear(float3 pq)
{
	const float m1 = 2610.0 / 4096.0 / 4;
	const float m2 = 2523.0 / 4096.0 * 128;
	const float c1 = 3424.0 / 4096.0;
	const float c2 = 2413.0 / 4096.0 * 32;
	const float c3 = 2392.0 / 4096.0 * 32;
	const float standardNits = 100.0;
	const float maxNits = 10000.0;
	float3 cp = pow(abs(pq), 1.0 / m2);
	return pow(max(cp - c1, 0.0) / (c2 - c3 * cp), 1.0 / m1) * maxNits / standardNits;
}

float3 Rec709ToRec2020(float3 color)
{
	static const float3x3 kMat =
	{
		0.627402, 0.329292, 0.043306,
		0.069095, 0.919544, 0.011360,
		0.016394, 0.088028, 0.895578
	};
	return mul(kMat, color);
}

float3 Rec2020ToRec709(float3 color)
{
	static const float3x3 kMat =
	{
		 1.660496, -0.587656, -0.072840,
		-0.124547,  1.132895, -0.008348,
		-0.018154, -0.100597,  1.118751
	};
	return mul(kMat, color);
}

#endif // COLORSPACE_HLSLI
//	EOF
