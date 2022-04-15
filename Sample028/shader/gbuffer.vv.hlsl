#include "common.hlsli"

struct VSInput
{
	float3	position	: POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD;
};

struct VSOutput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD;
	float4	currPos		: CURR_POS;
	float4	prevPos		: PREV_POS;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
ConstantBuffer<MeshCB>		cbMesh			: register(b1);

VSOutput main(const VSInput In)
{
	VSOutput Out;

	float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, cbMesh.mtxLocalToWorld);
	float4x4 mtxPrevLocalToProj = mul(cbScene.mtxPrevWorldToProj, cbMesh.mtxPrevLocalToWorld);

	Out.position = Out.currPos = mul(mtxLocalToProj, float4(In.position, 1));
	Out.prevPos = mul(mtxPrevLocalToProj, float4(In.position, 1));
	Out.normal = normalize(mul((float3x3)cbMesh.mtxLocalToWorld, In.normal));
	Out.tangent.xyz = normalize(mul((float3x3)cbMesh.mtxLocalToWorld, In.tangent.xyz));
	Out.tangent.w = In.tangent.w;
	Out.uv = In.uv;

	return Out;
}

//	EOF
