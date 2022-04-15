#include "common.hlsli"

struct VSInput
{
	float3	position	: POSITION;
	float2	uv			: TEXCOORD;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
ConstantBuffer<MeshCB>		cbMesh			: register(b1);

VSOutput main(const VSInput In)
{
	VSOutput Out;

	float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, cbMesh.mtxLocalToWorld);
	float4x4 mtxPrevLocalToProj = mul(cbScene.mtxPrevWorldToProj, cbMesh.mtxPrevLocalToWorld);

	Out.position = mul(mtxLocalToProj, float4(In.position, 1));
	Out.uv = In.uv;

	return Out;
}

//	EOF
