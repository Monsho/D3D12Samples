#include "common.hlsli"
#include "payload.hlsli"

#define TMax			10000.0

// global
RaytracingAccelerationStructure		Scene			: register(t0, space0);
Texture2D							WorldQuatTex	: register(t1);
Texture2D							DepthTex		: register(t2);
RWTexture2D<float>					RenderTarget	: register(u0);
ConstantBuffer<SceneCB>				cbScene			: register(b0);
ConstantBuffer<LightCB>				cbLight			: register(b1);


float RadicalInverseVdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley2D(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverseVdC(i));
}

float3 HemisphereSampleUniform(float u, float v)
{
	float phi = v * 2.0 * PI;
	float cosTheta = 1.0 - u;
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3 HemisphereSampleCos(float u, float v)
{
	float phi = v * 2.0 * PI;
	float cosTheta = sqrt(1.0 - u);
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float4 QuatMul(float4 q1, float4 q2)
{
	return float4(
		q2.xyz * q1.w + q1.xyz * q2.w + cross(q1.xyz, q2.xyz),
		q1.w * q2.w - dot(q1.xyz, q2.xyz)
		);
}

float4 QuatFromAngleAxis(float angle, float3 axis)
{
	float sn = sin(angle * 0.5);
	float cs = cos(angle * 0.5);
	return float4(axis * sn, cs);
}

float4 QuatFromTwoVector(float3 v1, float3 v2)
{
	float4 q;
	float d = dot(v1, v2);
	if (d < -0.999999)
	{
		float3 right = float3(1, 0, 0);
		float3 up = float3(0, 1, 0);
		float3 tmp = cross(right, v1);
		if (length(tmp) < 0.000001)
		{
			tmp = cross(up, v1);
		}
		tmp = normalize(tmp);
		q = QuatFromAngleAxis(PI, tmp);
	}
	else if (d > 0.999999)
	{
		q = float4(0, 0, 0, 1);
	}
	else
	{
		q.xyz = cross(v1, v2);
		q.w = 1 + d;
		q = normalize(q);
	}
	return q;
}

float3 QuatRotVector(float3 v, float4 r)
{
	float4 r_c = r * float4(-1, -1, -1, 1);
	return QuatMul(r, QuatMul(float4(v, 0), r_c)).xyz;
}

[shader("raygeneration")]
void DirectShadowRGS()
{
	// NormalとDepthをGBufferから取得
	uint2 index = DispatchRaysIndex().xy;
	float depth = DepthTex[index].r;
	if (depth >= 1.0)
	{
		RenderTarget[index] = 1.0;
		return;
	}

	float4 packed = WorldQuatTex[index];
	float4 quat = UnpackQuat(packed);
	float3 normal = ConvertVectorTangentToWorld(float3(0, 0, 1), quat);

	// ピクセル中心座標をクリップ空間座標に変換
	float2 xy = (float2)index + 0.5;
	float2 clipSpacePos = xy / float2(DispatchRaysDimensions().xy) * float2(2, -2) + float2(-1, 1);

	// クリップ空間座標をワールド空間座標に変換
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	// シャドウ用レイの生成
	float3 direction = -cbLight.lightDir.xyz;
	float3 origin = worldPos.xyz + normal * 1e-2;

	uint ray_flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

	// シャドウ計算
	RayDesc ray = { origin, 0.0, direction, TMax };
	HitPayload payload;
	payload.hitT = -1;
	TraceRay(Scene, ray_flags, ~0, kShadowContribution, kGeometricContributionMult, 0, ray, payload);

	// output result.
	RenderTarget[index] = (payload.hitT >= 0.0) ? 0.0 : 1.0;
}

[shader("miss")]
void DirectShadowMS(inout HitPayload payload : SV_RayPayload)
{
	// not implement.
}

// EOF
