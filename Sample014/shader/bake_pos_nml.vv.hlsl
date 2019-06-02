#include "common.hlsli"

struct VSInput
{
	float3	position	: POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	mesh_pos	: POSITION;
	float3	mesh_nml	: NORMAL;
};

VSOutput main(const VSInput In)
{
	VSOutput Out;

	Out.position = float4(In.uv * float2(2, -2) + float2(-1, 1), 0, 1);
	Out.mesh_pos = In.position;
	Out.mesh_nml = In.normal;

	return Out;
}

//	EOF
