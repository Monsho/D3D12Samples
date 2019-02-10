#include "common.hlsli"

struct HitData
{
	float	visible;
};

#define TMax			10000.0
#define MaxSample		512
#define	PI				3.14159265358979

// global
RaytracingAccelerationStructure		Scene			: register(t0, space0);
StructuredBuffer<float>				RandomTable		: register(t1);
Texture2D							GBufferTex		: register(t2);
Texture2D							DepthTex		: register(t3);
RWTexture2D<float4>					RenderTarget	: register(u0);
RWByteAddressBuffer					RandomSeed		: register(u1);
ConstantBuffer<SceneCB>				cbScene			: register(b0);

// local
Texture2D							texImage		: register(t4);
SamplerState						texImage_s		: register(s4);


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

float GetRandom()
{
	uint adr;
	RandomSeed.InterlockedAdd(0, 1, adr);
	return RandomTable[adr % 65536];
}

float3 Reflect(float3 v, float3 n)
{
	return v - dot(v, n) * n * 2.0;
}

[shader("raygeneration")]
void RayGenerator()
{
	float2 offset = { GetRandom(), GetRandom() };

	// NormalとDepthをGBufferから取得
	uint2 index = DispatchRaysIndex().xy;
	float3 normal = normalize(GBufferTex[index].rgb * 2.0 - 1.0);
	float depth = DepthTex[index].r;

	// ピクセル中心座標をクリップ空間座標に変換
	float2 xy = (float2)index + offset;
	float2 clipSpacePos = xy / float2(DispatchRaysDimensions().xy) * float2(2, -2) + float2(-1, 1);

	// クリップ空間座標をワールド空間座標に変換
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;
	worldPos.xyz += normal * 1e-4;

	// シャドウ用レイの生成
	float2 ham = Hammersley2D(cbScene.loopCount % MaxSample, MaxSample);
	float3 localDir = HemisphereSampleCos(ham.x * 1e-4, ham.y);
	float4 qRot = QuatFromTwoVector(float3(0, 0, 1), -cbScene.lightDir.xyz);
	float3 origin = worldPos.xyz;
	float3 direction = normalize(QuatRotVector(localDir, qRot));

	// シャドウ計算
	RayDesc ray = { origin, 0.0, direction, TMax };
	HitData shadowPay = { 0 };
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, shadowPay);

	// AO計算
	float ao = 0;
	uint sampleBase = cbScene.loopCount * cbScene.aoSampleCount;
	uint sampleMax = MaxSample * cbScene.aoSampleCount;
	for (uint i = 0; i < cbScene.aoSampleCount; i++)
	{
		ham = Hammersley2D((sampleBase + i) % sampleMax, sampleMax);
		localDir = HemisphereSampleUniform(ham.x, ham.y);
		qRot = QuatFromTwoVector(float3(0, 0, 1), normal);
		direction = QuatRotVector(localDir, qRot);
		RayDesc aoRay = { origin, 0.0, direction, cbScene.aoLength };
		HitData aoPay = { 0 };
		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, aoRay, aoPay);
		ao += aoPay.visible;
	}
	ao /= (float)cbScene.aoSampleCount;

	// 結果の生成
	float4 result = float4(shadowPay.visible, ao, 0, 0);

	// 前回までの結果とブレンド
	float4 prev = RenderTarget[index] * float(cbScene.loopCount);
	RenderTarget[index] = (prev + result) / float(cbScene.loopCount + 1);
}

[shader("closesthit")]
void ClosestHitProcessor(inout HitData payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
	payload.visible = 0;
}

[shader("miss")]
void MissProcessor(inout HitData payload : SV_RayPayload)
{
	payload.visible = 1;
}

// EOF
