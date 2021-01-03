#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD;
	float4	color		: COLOR;
	uint	materialId	: MATERIALID;
};

struct PSOutput
{
	float4	worldQuat		: SV_TARGET0;
	float2	uv				: SV_TARGET1;
	float4	derivativeUV	: SV_TARGET2;
	uint	materialId		: SV_TARGET3;
	float4	multColor		: SV_TARGET4;
};

ConstantBuffer<SceneCB>					cbScene			: register(b0);

Texture2D		texImage		: register(t0);
SamplerState	texImage_s		: register(s0);

PSOutput main(PSInput In)
{
	float a = texImage.Sample(texImage_s, In.uv).a;
	clip(a < 0.5 ? -1 : 1);

	PSOutput Out = (PSOutput)0;
	Out.multColor = (1.0).xxxx;
	if (cbScene.isMeshletColor)
	{
		Out.multColor = In.color;
	}

	float3 T, B, N;
	N = normalize(In.normal);
	B = normalize(cross(N, In.tangent.xyz));
	T = cross(B, N);
	Out.worldQuat = PackQuat(TangentSpaceToQuat(T, B, N));

	Out.uv = frac(In.uv);
	Out.derivativeUV.xy = ddx(In.uv);
	Out.derivativeUV.zw = ddy(In.uv);

	// material ID and binormal sign.
	Out.materialId = (In.materialId & 0x7fff) | (In.tangent.w > 0.0 ? 0x8000 : 0x0);

	return Out;
}
