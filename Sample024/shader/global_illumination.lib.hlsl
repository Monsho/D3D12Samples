#include "common.hlsli"
#include "payload.hlsli"

#define TMax			10000.0

// global
RaytracingAccelerationStructure			Scene			: register(t0, space0);
Texture2D								WorldNormalTex	: register(t1);
Texture2D<float>						DepthTex		: register(t2);
Texture2D<float>						BlueNoiseTex	: register(t3);
RWTexture2D<float3>						RenderTarget	: register(u0);
ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);
ConstantBuffer<GlobalIlluminationCB>	cbGI			: register(b2);


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
void GlobalIlluminationRGS()
{
	// NormalとDepthをGBufferから取得
	uint2 index = DispatchRaysIndex().xy;
	float depth = DepthTex[index].r;
	if (depth >= 1.0)
	{
		RenderTarget[index] = 1.0;
		return;
	}

	uint offset = BlueNoiseTex[index % 64] * 256;

	float3 normal = WorldNormalTex[index].xyz * 2.0 - 1.0;

	// ピクセル中心座標をクリップ空間座標に変換
	float2 xy = (float2)index + 0.5;
	float2 clipSpacePos = xy / float2(DispatchRaysDimensions().xy) * float2(2, -2) + float2(-1, 1);

	// クリップ空間座標をワールド空間座標に変換
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;
	float3 origin = worldPos.xyz + normal * 1e-2;

	uint ray_flags = RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
	const uint kSampleMax = 1024;
	uint sampleBase = cbGI.totalSampleCount;
	float3 totalColor = (float3)0.0;
	for (uint i = 0; i < cbGI.sampleCount; i++)
	{
		uint seed = (sampleBase + i + offset) % kSampleMax;
		float2 ham = Hammersley2D(seed, kSampleMax);
		float3 localDir = HemisphereSampleCos(ham.x, ham.y);
		float4 qRot = QuatFromTwoVector(float3(0, 0, 1), normal);
		float3 direction = QuatRotVector(localDir, qRot);
		RayDesc ray = { origin, 0.0, direction, TMax };
		MaterialPayload payload = (MaterialPayload)0;
		TraceRay(Scene, ray_flags, ~0, kMaterialContribution, kGeometricContributionMult, 0, ray, payload);

		MaterialParam mat_param = (MaterialParam)0;
		DecodeMaterialPayload(payload, mat_param);

		if (payload.hitT >= 0.0)
		{
			// prepare shadow ray.
			float3 shadow_dir = -cbLight.lightDir.xyz;
			float3 shadow_pos = origin + direction * payload.hitT;
			float3 shadow_orig = shadow_pos + mat_param.normal * 1e-2;

			// shadow ray trace.
			RayDesc shadow_ray = { shadow_orig, 0.0, shadow_dir, TMax };
			HitPayload shadow_pay;
			shadow_pay.hitT = -1;
			TraceRay(Scene, ray_flags, ~0, kShadowContribution, kGeometricContributionMult, 1, shadow_ray, shadow_pay);
			float shadow_factor = (shadow_pay.hitT >= 0.0) ? 0.0 : 1.0;

			// lighting.
			float3 N = mat_param.normal;
			float3 L = -cbLight.lightDir.xyz;
			float3 DiffuseColor = lerp(mat_param.baseColor.rgb, 0.0, mat_param.metallic);
			float3 LightResult = (saturate(dot(N, L)) * shadow_factor * cbLight.lightColor.rgb + SkyColor(mat_param.normal) * cbLight.skyPower) * DiffuseColor;
			float attn = saturate(1.0 / (payload.hitT * payload.hitT));
			//totalColor += (LightResult * localDir.z * attn);
			totalColor += LightResult * localDir.z;
		}
		else
		{
			totalColor += mat_param.baseColor.rgb;
		}
	}

	// output result.
	float div = 1.0 / float(sampleBase + cbGI.sampleCount);
	float3 prev_color = RenderTarget[index];
	RenderTarget[index] = prev_color * (float(sampleBase) * div) + (totalColor * div);
	//RenderTarget[index] = RenderTarget[index] + totalColor;
}

[shader("miss")]
void GlobalIlluminationMS(inout MaterialPayload payload : SV_RayPayload)
{
	MaterialParam mat_param = (MaterialParam)0;
	mat_param.baseColor.rgb = SkyColor(WorldRayDirection()) * cbLight.skyPower;
	EncodeMaterialPayload(mat_param, payload);
	payload.hitT = -1.0;
}

[shader("miss")]
void DirectShadowMS(inout HitPayload payload : SV_RayPayload)
{
	// not implement.
}

// EOF
