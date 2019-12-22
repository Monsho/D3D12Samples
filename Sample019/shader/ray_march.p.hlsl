#include "common.hlsli"

struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>			cbScene			: register(b0);
ConstantBuffer<BoxInfo>			cbBoxInfo		: register(b1);
ConstantBuffer<BoxTransform>	cbBoxTrans		: register(b2);
ConstantBuffer<BoxTransform>	cbBoxMoveTrans	: register(b3);

Texture3D<float>	texSdf			: register(t0);
SamplerState		texSdf_s		: register(s0);

float sdVolumeTexture(float3 p, float4x4 mtxLocalToWorld, float4x4 mtxWorldToLocal, float3 centerPos, float extent, float sizeVoxel)
{
	// box - point distance.
	float3 cpWS = mul(mtxLocalToWorld, float4(centerPos, 1));
	float3 pLS = p - cpWS;
	float3 x = mtxLocalToWorld._11_12_13 * extent;
	float3 y = mtxLocalToWorld._21_22_23 * extent;
	float3 z = mtxLocalToWorld._31_32_33 * extent;
	float lx = length(x);
	float ly = length(y);
	float lz = length(z);
	x /= lx;
	y /= ly;
	z /= lz;
	float dist = 0;
	float l = dot(pLS, x);
	float al = abs(l);
	if (al > lx)
	{
		float dl = al - lx;
		dist = dl;
		p -= sign(l) * dl * x;
	}
	l = dot(pLS, y);
	al = abs(l);
	if (al > ly)
	{
		float dl = al - ly;
		dist = sqrt(dist * dist + dl * dl);
		p -= sign(l) * dl * y;
	}
	l = dot(pLS, z);
	al = abs(l);
	if (al > lz)
	{
		float dl = al - lz;
		dist = sqrt(dist * dist + dl * dl);
		p -= sign(l) * dl * z;
	}

	// sdf - point distance.
	float4 local_origin = mul(mtxWorldToLocal, float4(p, 1));
	float3 origin = (local_origin.xyz / local_origin.w) - centerPos;
	float inv_box_width = 1 / (extent * 2);
	float max_distance = extent * 3 * 2;
	float3 uvw = (origin + extent) * inv_box_width;
	return texSdf.SampleLevel(texSdf_s, uvw, 0) * max_distance + dist;
}

float opExponentialUnion(float a, float b, float k)
{
	float res = exp2(-k * a) + exp2(-k * b);
	return -log2(res) / k;
}

float opPolynomialUnion(float d1, float d2, float k)
{
	float h = saturate(0.5 + 0.5 * (d2 - d1) / k);
	return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float opPowerUnion(float a, float b, float k)
{
	a = pow(a, k); b = pow(b, k);
	return pow((a * b) / (a + b), 1.0 / k);
}

float sdScene(float3 p)
{
	float d1 = sdVolumeTexture(p, cbBoxTrans.mtxLocalToWorld, cbBoxTrans.mtxWorldToLocal, cbBoxInfo.centerPos, cbBoxInfo.extentWhole, cbBoxInfo.sizeVoxel);
	float d2 = sdVolumeTexture(p, cbBoxMoveTrans.mtxLocalToWorld, cbBoxMoveTrans.mtxWorldToLocal, cbBoxInfo.centerPos, cbBoxInfo.extentWhole, cbBoxInfo.sizeVoxel);
	return opPolynomialUnion(d1, d2, 0.4);
	//return opExponentialUnion(d1, d2, 8);
}

float4 main(PSInput In) : SV_TARGET0
{
	// generate ray direction.
	float2 posCS = In.uv * float2(2, -2) + float2(-1, 1);
	float4 posWS = mul(cbScene.mtxProjToWorld, float4(posCS, 1, 1));
	posWS.xyz /= posWS.w;
	float3 origin = cbScene.camPos.xyz;
	float3 direction = posWS.xyz - origin;

	float limit_distance = length(direction);
	direction /= limit_distance;
	float sum_d = 0;
	[loop]
	for (int i = 0; i < 128; i++)
	{
		float d = sdScene(origin);
		if (d <= 1e-2)
		{
			float nx = d - sdScene(origin - float3(1e-2, 0, 0));
			float ny = d - sdScene(origin - float3(0, 1e-2, 0));
			float nz = d - sdScene(origin - float3(0, 0, 1e-2));
			float3 normal = normalize(float3(nx, ny, nz));
			return float4(normal.yyy * 0.5 + 0.5, 1);
		}
		sum_d += d;
		if (sum_d > limit_distance)
		{
			break;
		}
		origin += direction * d;
	}
	clip(-1);
	return (0).xxxx;
}
