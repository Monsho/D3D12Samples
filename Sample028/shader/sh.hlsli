// from https://github.com/TheRealMJP/LowResRendering

#ifndef _SH_HLSLI_
#define _SH_HLSLI_

static const float CosineA0 = 1.0;
static const float CosineA1 = 2.0 / 3.0;
static const float CosineA2 = 0.25;

struct SH9
{
	float	Coefficients[9];
};

struct SH9Color
{
	float3	Coefficients[9];
};

SH9 ProjectOntoSH9(float3 dir, float intensity, float cosa0, float cosa1, float cosa2)
{
	SH9 sh;

	// Band 0
	sh.Coefficients[0] = 0.282095f * cosa0 * intensity;

	// Band 1
	sh.Coefficients[1] = 0.488603f * dir.y * cosa1 * intensity;
	sh.Coefficients[2] = 0.488603f * dir.z * cosa1 * intensity;
	sh.Coefficients[3] = 0.488603f * dir.x * cosa1 * intensity;

	// Band 2
	sh.Coefficients[4] = 1.092548f * dir.x * dir.y * cosa2 * intensity;
	sh.Coefficients[5] = 1.092548f * dir.y * dir.z * cosa2 * intensity;
	sh.Coefficients[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f) * cosa2 * intensity;
	sh.Coefficients[7] = 1.092548f * dir.x * dir.z * cosa2 * intensity;
	sh.Coefficients[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y) * cosa2 * intensity;

	return sh;
}

SH9Color ProjectOntoSH9Color(float3 dir, float3 color, float cosa0, float cosa1, float cosa2)
{
	SH9Color sh;

	// Band 0
	sh.Coefficients[0] = 0.282095f * cosa0 * color;

	// Band 1
	sh.Coefficients[1] = 0.488603f * dir.y * cosa1 * color;
	sh.Coefficients[2] = 0.488603f * dir.z * cosa1 * color;
	sh.Coefficients[3] = 0.488603f * dir.x * cosa1 * color;

	// Band 2
	sh.Coefficients[4] = 1.092548f * dir.x * dir.y * cosa2 * color;
	sh.Coefficients[5] = 1.092548f * dir.y * dir.z * cosa2 * color;
	sh.Coefficients[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f) * cosa2 * color;
	sh.Coefficients[7] = 1.092548f * dir.x * dir.z * cosa2 * color;
	sh.Coefficients[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y) * cosa2 * color;

	return sh;
}

float DotSH9(SH9 a, SH9 b)
{
	float result = 0.0;

	[unroll]
	for(uint i = 0; i < 9; ++i)
		result += a.Coefficients[i] * b.Coefficients[i];

	return result;
}

float3 DotSH9(SH9Color a, SH9 b)
{
	float3 result = 0.0;

	[unroll]
	for(uint i = 0; i < 9; ++i)
		result += a.Coefficients[i] * b.Coefficients[i];

	return result;
}


float3 DotSH9(SH9 a, SH9Color b)
{
	float3 result = 0.0;

	[unroll]
	for(uint i = 0; i < 9; ++i)
		result += a.Coefficients[i] * b.Coefficients[i];

	return result;
}


float3 DotSH9(SH9Color a, SH9Color b)
{
	float3 result = 0.0;

	[unroll]
	for(uint i = 0; i < 9; ++i)
		result += a.Coefficients[i] * b.Coefficients[i];

	return result;
}

SH9Color AddSH9(SH9Color a, SH9Color b)
{
	SH9Color result = (SH9Color)0;
	[unroll]
	for(uint i = 0; i < 9; ++i)
		result.Coefficients[i] = a.Coefficients[i] + b.Coefficients[i];

	return result;
}

SH9Color ScaleSH9(SH9Color a, float b)
{
	SH9Color result = (SH9Color)0;
	[unroll]
	for(uint i = 0; i < 9; ++i)
		result.Coefficients[i] = a.Coefficients[i] * b;

	return result;
}

float EvalSH9(float dir, SH9 sh)
{
	SH9 dirSH = ProjectOntoSH9(dir, 1.0, 1.0, 1.0, 1.0);
	return DotSH9(dirSH, sh);
}

float3 EvalSH9(in float3 dir, in SH9Color sh)
{
	SH9Color dirSH = ProjectOntoSH9Color(dir, 1.0, 1.0, 1.0, 1.0);
	return DotSH9(dirSH, sh);
}

float EvalSH9Cosine(in float3 dir, in SH9 sh)
{
	SH9 dirSH = ProjectOntoSH9(dir, 1.0f, CosineA0, CosineA1, CosineA2);
	return DotSH9(dirSH, sh);
}

float3 EvalSH9Cosine(in float3 dir, in SH9Color sh)
{
	SH9Color dirSH = ProjectOntoSH9Color(dir, 1.0f, CosineA0, CosineA1, CosineA2);
	return DotSH9(dirSH, sh);
}

#endif // _SH_HLSLI_
// EOF
