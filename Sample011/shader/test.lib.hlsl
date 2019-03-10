struct SceneCB
{
	float4x4	mtxProjToWorld;
	float4		camPos;
	float4		lightDir;
	float4		lightColor;
	float		skyPower;
	uint		loopCount;
};

struct HitData
{
	float4	color;
	int		refl_count;
	float	minT;
};

#define TMax			10000.0
#define MaxReflCount	8
#define MaxSample		2048
#define	PI				3.14159265358979

// global
RaytracingAccelerationStructure		Scene			: register(t0, space0);
StructuredBuffer<float>				RandomTable		: register(t1);
RWTexture2D<float4>					RenderTarget	: register(u0);
RWByteAddressBuffer					RandomSeed		: register(u1);
ConstantBuffer<SceneCB>				cbScene			: register(b0);

// local
ByteAddressBuffer					Indices			: register(t2);
StructuredBuffer<float3>			VertexNormal	: register(t3);
StructuredBuffer<float2>			VertexUV		: register(t4);
Texture2D							texImage		: register(t5);
SamplerState						samImage		: register(s0);


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

float3 SkyColor(float3 w_dir)
{
	float3 t = w_dir.y * 0.5 + 0.5;
	return saturate((1.0).xxx * (1.0 - t) + float3(0.5, 0.7, 1.0) * t);
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

	// ピクセル中心座標をクリップ空間座標に変換
	uint2 index = DispatchRaysIndex().xy;
	float2 xy = (float2)index + offset;
	float2 clipSpacePos = xy / float2(DispatchRaysDimensions().xy) * float2(2, -2) + float2(-1, 1);

	// クリップ空間座標をワールド空間座標に変換
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, 1, 1));

	// ワールド空間座標とカメラ位置からレイを生成
	worldPos.xyz /= worldPos.w;
	float3 origin = cbScene.camPos.xyz;
	float3 direction = normalize(worldPos.xyz - origin);

	// Let's レイトレ！
	RayDesc ray = { origin, 0.0, direction, TMax };
	HitData payload = { float4(0, 0, 0, 0), 0, 0 };
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 2, 0, ray, payload);

	// 前回までの結果とブレンド
	float4 prev = RenderTarget[index] * float(cbScene.loopCount);
	RenderTarget[index] = (prev + payload.color) / float(cbScene.loopCount + 1);
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
	normal = normalize(normal);

	payload.minT = RayTCurrent();
	if (payload.refl_count < MaxReflCount)
	{
		float rnd = GetRandom();
		uint i = (cbScene.loopCount + uint(rnd * MaxSample)) % MaxSample;
		float2 ham = Hammersley2D(i, MaxSample);
		float3 localDir = HemisphereSampleUniform(ham.x, ham.y);
		float4 qRot = QuatFromTwoVector(float3(0, 0, 1), normal);
		float3 traceDir = QuatRotVector(localDir, qRot);

		float3 attenuation = texImage.SampleLevel(samImage, uv, 0.0).rgb;

		// 反射ベクトルに対してレイトレ
		float3 origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * 0.01;
		RayDesc ray = { origin, 0, traceDir, TMax };
		HitData refl_payload = { float4(0, 0, 0, 0), payload.refl_count + 1, 0 };
		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 2, 0, ray, refl_payload);

		// シャドウ計算用のレイトレ
		localDir = HemisphereSampleCos(ham.x * 1e-4, ham.y);
		qRot = QuatFromTwoVector(float3(0, 0, 1), -cbScene.lightDir.xyz);
		traceDir = QuatRotVector(localDir, qRot);
		RayDesc shadow_ray = { origin, 0.0, traceDir, TMax };
		HitData shadow_payload = { float4(0, 0, 0, 0), payload.refl_count + 1, 0 };
		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 1, 2, 1, shadow_ray, shadow_payload);

		// 直接光と間接光の結果を合算
		float l = 1;
		//float l = refl_payload.minT < 0.0 ? 1.0 : refl_payload.minT * 1000.0;
		payload.color.rgb = (saturate(dot(normal, -cbScene.lightDir.xyz)) * cbScene.lightColor.rgb * shadow_payload.color.rgb + refl_payload.color.rgb * (1.0 / pow(l, 16))) * attenuation;
	}
	else
	{
		payload.color = (0.0).xxxx;
	}
}

[shader("closesthit")]
void ClosestHitShadowProcessor(inout HitData payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
	payload.color = (0.0).xxxx;
}

[shader("miss")]
void MissProcessor(inout HitData payload : SV_RayPayload)
{
	float3 col = SkyColor(WorldRayDirection()) * cbScene.skyPower;
	payload.color = float4(col, 1.0);
	payload.minT = -1.0;
}

[shader("miss")]
void MissShadowProcessor(inout HitData payload : SV_RayPayload)
{
	payload.color = (1.0).xxxx;
}

// EOF
