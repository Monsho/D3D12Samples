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

	float4	currPosCS	: CURR_POS_CS;
	float4	prevPosCS	: PREV_POS_CS;
};

ConstantBuffer<SceneCB>		cbScene			: register(b0);
ConstantBuffer<MeshCB>		cbMesh			: register(b1);

VSOutput main(const VSInput In)
{
	VSOutput Out;

	float4x4 mtxLocalToProj = mul(cbScene.mtxWorldToProj, cbMesh.mtxLocalToWorld);
	float4x4 mtxPrevLocalToProj = mul(cbScene.mtxPrevWorldToProj, cbMesh.mtxPrevLocalToWorld);

	Out.position = mul(mtxLocalToProj, float4(In.position, 1));
	Out.normal = normalize(mul((float3x3)cbMesh.mtxLocalToWorld, In.normal));
	Out.tangent.xyz = normalize(mul((float3x3)cbMesh.mtxLocalToWorld, In.tangent.xyz));
	Out.tangent.w = In.tangent.w;
	Out.uv = In.uv;

	Out.currPosCS = Out.position;
	Out.prevPosCS = mul(mtxPrevLocalToProj, float4(In.position, 1));

	return Out;
}

//	EOF
