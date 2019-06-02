#include "common.hlsli"

struct VSInput
{
	float3	position	: POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD0;
	float2	unique_uv	: TEXCOORD1;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float2	uv			: TEXCOORD0;
	float2	unique_uv	: TEXCOORD1;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);

VSOutput main(const VSInput In)
{
	VSOutput Out;

	Out.position = mul(cbScene.mtxWorldToProj, float4(In.position, 1));
	Out.normal = In.normal;
	Out.uv = In.uv;
	Out.unique_uv = In.unique_uv;

	return Out;
}

//	EOF
