#include "common.hlsli"
#include "sh.hlsli"

#ifndef CUBEMAP_WIDTH
#	define CUBEMAP_WIDTH		128
#endif

RWStructuredBuffer<SH9Color>	rwSH9Color		: register(u0);
RWStructuredBuffer<SH9Color>	rwWorkSH9		: register(u1);
RWStructuredBuffer<float>		rwWorkWeight	: register(u2);

Texture2D<float4>		texPanorama		: register(t0);
SamplerState			samPanorama		: register(s0);

groupshared SH9Color	gsXMerge[CUBEMAP_WIDTH];
groupshared float		gsWeightSum[CUBEMAP_WIDTH];

float3 UVToDirection(float u, float v, uint face)
{
	float3 dir;

	switch (face)
	{
	case 0: dir = float3(1.0, -v, -u); break;
	case 1: dir = float3(-1.0, -v, u); break;
	case 2: dir = float3(u, 1.0, v); break;
	case 3: dir = float3(u, -1.0, -v); break;
	case 4: dir = float3(u, -v, 1.0); break;
	case 5: dir = float3(-u, -v, -1.0); break;
	}

	return normalize(dir);
}

float2 DirToPanoramaUV(float3 dir)
{
	return float2(
		0.5f + 0.5f * atan2(dir.z, dir.x) / PI,
		acos(dir.y) / PI);
}

float3 SamplePanorama(float3 dir)
{
	float2 uv = DirToPanoramaUV(dir);
	return texPanorama.SampleLevel(samPanorama, uv, 0).rgb;
}

[numthreads(CUBEMAP_WIDTH, 1, 1)]
void ComputePerFace(uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
	uint y = gtid.x;
	uint face = gid.x;

	// merge on X direction.
	SH9Color sh = (SH9Color)0;
	float weightSum = 0;
	float v = (y + 0.5) / CUBEMAP_WIDTH;
	v = v * 2.0 - 1.0;
	for (uint x = 0; x < CUBEMAP_WIDTH; x++)
	{
		float u = (x + 0.5) / CUBEMAP_WIDTH;
		u = u * 2.0 - 1.0;
		float temp = 1.0 + u * u + v * v;
		float weight = 4.0 / (sqrt(temp) * temp);

		float3 dir = UVToDirection(u, v, face);
		float3 color = SamplePanorama(dir);
		sh = AddSH9(sh, ScaleSH9(ProjectOntoSH9Color(dir, color, 1.0, 1.0, 1.0), weight));
		weightSum += weight;
	}
	gsXMerge[y] = sh;
	gsWeightSum[y] = weightSum;

	GroupMemoryBarrierWithGroupSync();

	// merge on Y direction.
	if (y == 0)
	{
		SH9Color result = (SH9Color)0;
		float w = 0;
		for (uint yy = 0; yy < CUBEMAP_WIDTH; yy++)
		{
			result = AddSH9(result, gsXMerge[yy]);
			w += gsWeightSum[yy];
		}
		rwWorkSH9[face] = result;
		rwWorkWeight[face] = w;
	}
}

[numthreads(1, 1, 1)]
void ComputeAll()
{
	SH9Color result = (SH9Color)0;
	float w = 0;
	for (uint f = 0; f < 6; f++)
	{
		result = AddSH9(result, rwWorkSH9[f]);
		w += rwWorkWeight[f];
	}

	rwSH9Color[0] = ScaleSH9(result, 4.0 * PI / w);
}

//	EOF
