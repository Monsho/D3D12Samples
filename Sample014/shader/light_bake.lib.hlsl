#include "common.hlsli"

struct HitData
{
	float4	material_color;
	float3	material_normal;
	float3	next_ray_origin;
	float3	next_ray_dir;
	uint	is_hit;
};

#define TMax			10000.0
#define MaxSample		512
#define	PI				3.14159265358979

// global
RaytracingAccelerationStructure		Scene			: register(t0, space0);
StructuredBuffer<float>				RandomTable		: register(t1);
Texture2D<float4>					SourcePosition	: register(t2);
Texture2D<float4>					SourceNormal	: register(t3);
RWTexture2D<float4>					DestLightMap	: register(u0);
RWByteAddressBuffer					RandomSeed		: register(u1);
ConstantBuffer<SceneCB>				cbScene			: register(b0);
ConstantBuffer<TimeCB>				cbTime			: register(b1);

// local
ByteAddressBuffer					Indices			: register(t4);
StructuredBuffer<float3>			VertexPosition	: register(t5);
StructuredBuffer<float3>			VertexNormal	: register(t6);
StructuredBuffer<float2>			VertexUV		: register(t7);
Texture2D							texImage		: register(t8);
SamplerState						texImage_s		: register(s0);


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

uint3 GetTriangleIndices2byte(uint offset)
{
	uint alignedOffset = offset & ~0x3;
	uint2 indices4 = Indices.Load2(alignedOffset);

	uint3 ret;
	if (alignedOffset == offset)
	{
		ret.x = indices4.x & 0xffff;
		ret.y = (indices4.x >> 16) & 0xffff;
		ret.z = indices4.y & 0xffff;
	}
	else
	{
		ret.x = (indices4.x >> 16) & 0xffff;
		ret.y = indices4.y & 0xffff;
		ret.z = (indices4.y >> 16) & 0xffff;
	}

	return ret;
}

uint3 Get16bitIndices(uint primIdx)
{
	uint indexOffset = primIdx * 2 * 3;
	return GetTriangleIndices2byte(indexOffset);
}

uint3 Get32bitIndices(uint primIdx)
{
	uint indexOffset = primIdx * 4 * 3;
	uint3 ret = Indices.Load3(indexOffset);
	return ret;
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

void CalcTangentSpace(float3 pos0, float3 pos1, float3 pos2, float2 uv0, float2 uv1, float2 uv2, out float3 T, out float3 B)
{
	float3 q1 = pos1 - pos0;
	float3 q2 = pos2 - pos0;
	float2 st1 = uv1 - uv0;
	float2 st2 = uv2 - uv0;
	float div = 1.0 / (st1.x * st2.y - st2.x * st1.y);
	float2 t = float2(st2.y, -st1.y) * div;
	float2 s = float2(-st2.x, st1.x) * div;
	T = t.x * q1 + t.y * q2;
	B = s.x * q1 + s.y * q2;
	T = normalize(T);
	B = normalize(B);
}

[shader("raygeneration")]
void RayGenerator()
{
	// インデックス番号から必要な頂点情報を取得
	uint2 index = DispatchRaysIndex().xy + cbTime.blockStart;
	float3 worldPos = SourcePosition[index].xyz;
	float3 normal = SourceNormal[index].xyz;

	if (SourcePosition[index].a <= 0.0)
		return;

	worldPos = mul(cbScene.mtxWorld, float4(worldPos, 1)).xyz;
	normal = normalize(mul((float3x3)cbScene.mtxWorld, normal));

	// 開始時のレイを生成する
	float rnd = GetRandom();
	uint sampleCnt = (cbTime.loopCount + uint(rnd * MaxSample)) % MaxSample;
	float2 ham = Hammersley2D(sampleCnt, MaxSample);
	float3 localDir = HemisphereSampleUniform(ham.x, ham.y);
	float4 qRot = QuatFromTwoVector(float3(0, 0, 1), normal);
	float3 traceDir = QuatRotVector(localDir, qRot);

	float3 origin = worldPos + normal * 1.0f;
	float3 direction = traceDir;

	// Let's レイトレ！
	RayDesc ray = { origin, 0.0, direction, TMax };
	HitData payload;
	float3 Irradiance = (0.0).xxx;
	float3 Radiance = (1.0).xxx;
	for (uint i = 0; i < cbScene.maxBounces; ++i)
	{
		// マテリアルに対するレイトレ
		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 2, 0, ray, payload);

		if (payload.is_hit)
		{
			// シャドウ計算用のレイトレ
			float rnd = GetRandom();
			uint i = (cbTime.loopCount + uint(rnd * MaxSample)) % MaxSample;
			float2 ham = Hammersley2D(i, MaxSample);
			float3 localDir = HemisphereSampleCos(ham.x * 1e-4, ham.y);
			float4 qRot = QuatFromTwoVector(float3(0, 0, 1), -cbScene.lightDir.xyz);
			float3 traceDir = QuatRotVector(localDir, qRot);
			RayDesc shadow_ray = { payload.next_ray_origin, 0.0, traceDir, TMax };
			HitData shadow_payload;
			shadow_payload.material_color = (0.0).xxxx;
			TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 1, 2, 1, shadow_ray, shadow_payload);

			// マテリアル計算
			Radiance *= payload.material_color.rgb;
			float3 R_direct = saturate(dot(payload.material_normal, -cbScene.lightDir.xyz)) * cbScene.lightColor.rgb * shadow_payload.material_color.rgb * Radiance;
			Irradiance += R_direct;

			RayDesc next_ray = { payload.next_ray_origin, 0.0, payload.next_ray_dir, TMax };
			ray = next_ray;
		}
		else
		{
			Radiance *= payload.material_color.rgb;
			Irradiance += Radiance;
			break;
		}
	}

	// 前回までの結果とブレンド
	//DestLightMap[index] = float4(normal * 0.5 + 0.5, 1);
	float3 prev = DestLightMap[index].rgb * float(cbTime.loopCount);
	DestLightMap[index] = float4((prev + Irradiance) / float(cbTime.loopCount + 1), 1);
}

[shader("closesthit")]
void ClosestHitProcessor(inout HitData payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
	uint3 indices = Get32bitIndices(PrimitiveIndex());

	float2 uvs[3] = {
		VertexUV[indices.x],
		VertexUV[indices.y],
		VertexUV[indices.z]
	};
	float2 uv = uvs[0] +
		attr.barycentrics.x * (uvs[1] - uvs[0]) +
		attr.barycentrics.y * (uvs[2] - uvs[0]);
	float3 normal = VertexNormal[indices.x] +
		attr.barycentrics.x * (VertexNormal[indices.y] - VertexNormal[indices.x]) +
		attr.barycentrics.y * (VertexNormal[indices.z] - VertexNormal[indices.x]);
	normal = normalize(mul((float3x3)cbScene.mtxWorld, normal));

	float minT = RayTCurrent();
	float rnd = GetRandom();
	uint i = (cbTime.loopCount + uint(rnd * MaxSample)) % MaxSample;
	float2 ham = Hammersley2D(i, MaxSample);
	float3 localDir = HemisphereSampleUniform(ham.x, ham.y);
	float4 qRot = QuatFromTwoVector(float3(0, 0, 1), normal);
	float3 traceDir = QuatRotVector(localDir, qRot);

	payload.material_normal = normal;
	payload.material_color = texImage.SampleLevel(texImage_s, uv, 0.0);

	// 反射ベクトルを計算
	payload.next_ray_origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * 0.01;
	payload.next_ray_dir = traceDir;

	payload.is_hit = 1;
}

[shader("anyhit")]
void AnyHitProcessor(inout HitData payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
	uint3 indices = Get32bitIndices(PrimitiveIndex());

	float2 uvs[3] = {
		VertexUV[indices.x],
		VertexUV[indices.y],
		VertexUV[indices.z]
	};
	float2 uv = uvs[0] +
		attr.barycentrics.x * (uvs[1] - uvs[0]) +
		attr.barycentrics.y * (uvs[2] - uvs[0]);

	float opacity = texImage.SampleLevel(texImage_s, uv, 0.0).a;
	if (opacity < 0.33)
	{
		IgnoreHit();
	}
}

[shader("miss")]
void MissProcessor(inout HitData payload : SV_RayPayload)
{
	float3 col = SkyColor(WorldRayDirection()) * cbScene.skyPower;
	payload.material_color = float4(col, 1);
	payload.is_hit = 0;
}

[shader("miss")]
void MissShadowProcessor(inout HitData payload : SV_RayPayload)
{
	payload.material_color = (1.0).xxxx;
}

// EOF
