struct SceneCB
{
	float4x4	mtxProjToWorld;
	float4		camPos;
	float4		lightDir;
	float4		lightColor;
	uint		loopCount;
};

struct HitData
{
	float4	color;
	uint	refl_count;
};

struct MyAttribute
{
	float3		normal;
};

struct RayData
{
	float3	origin;
	float3	dir;
};

struct HitRecord
{
	float3	w_pos;
	float3	w_normal;
	float	t;
};

struct Sphere
{
	float3	center;
	float	radius;
};

struct Instance
{
	float4x4	mtxLocalToWorld;
	float4		color;
	uint		material;
};

#define TMax			10000.0
#define MaxReflCount	30
#define MaxSample		512
#define	PI				3.14159265358979

// global
RaytracingAccelerationStructure		Scene			: register(t0, space0);
StructuredBuffer<Instance>			Instances		: register(t1);
StructuredBuffer<float>				RandomTable		: register(t2);
RWTexture2D<float4>					RenderTarget	: register(u0);
RWByteAddressBuffer					RandomSeed		: register(u1);
ConstantBuffer<SceneCB>				cbScene			: register(b0);


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

bool IntersectToSphere(RayData r, float t_min, float t_max, Sphere s, out HitRecord rec)
{
	float3 oc = r.origin - s.center;
	float a = dot(r.dir, r.dir);
	float b = dot(oc, r.dir);
	float c = dot(oc, oc) - s.radius * s.radius;
	float discriminant = b * b - a * c;

	if (discriminant > 0.0)
	{
		float tmp = (-b - sqrt(b * b - a * c)) / a;
		if (t_min < tmp && tmp < t_max)
		{
			rec.t = tmp;
			rec.w_pos = r.dir * tmp + r.origin;
			rec.w_normal = (rec.w_pos - s.center) / s.radius;
			return true;
		}
		tmp = (-b + sqrt(b * b - a * c)) / a;
		if (t_min < tmp && tmp < t_max)
		{
			rec.t = tmp;
			rec.w_pos = r.dir * tmp + r.origin;
			rec.w_normal = (rec.w_pos - s.center) / s.radius;
			return true;
		}
	}
	return false;
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

bool Refract(float3 v, float3 n, float ni_over_nt, out float3 refracted)
{
	float3 uv = normalize(v);
	float dt = dot(uv, n);
	float discriminant = 1.0 - ni_over_nt * ni_over_nt * (1.0 - dt * dt);
	if (discriminant > 0.0)
	{
		refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
		return true;
	}
	return false;
}

float Schlick(float cosine, float ref_idx)
{
	float r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
	r0 = r0 * r0;
	float oc = 1.0 - cosine;
	float oc2 = oc * oc;
	return r0 + (1.0 - r0) * oc2 * oc2 * oc;
}

[shader("raygeneration")]
void RayGenerateProc()
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
	HitData payload = { float4(0, 0, 0, 0), 0 };
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

	// 前回までの結果とブレンド
	float4 prev = RenderTarget[index] * float(cbScene.loopCount);
	RenderTarget[index] = (prev + payload.color) / float(cbScene.loopCount + 1);
}

[shader("intersection")]
void IntersectionSphereProc()
{
	Instance instance = Instances[PrimitiveIndex()];

	RayData ray;
	ray.origin = WorldRayOrigin();
	ray.dir = WorldRayDirection();

	float t_min = RayTMin();
	float t_max = TMax;

	Sphere s;
	s.center = instance.mtxLocalToWorld._m03_m13_m23;
	s.radius = instance.mtxLocalToWorld._m00;

	HitRecord rec;
	if (IntersectToSphere(ray, t_min, t_max, s, rec))
	{
		//if (dot(rec.w_normal, ray.dir) <= 0.0)
		{
			MyAttribute attr;
			attr.normal = rec.w_normal;
			ReportHit(rec.t, 0, attr);
		}
	}
}

[shader("closesthit")]
void ClosestHitProc(inout HitData payload : SV_RayPayload, in MyAttribute attr : SV_IntersectionAttributes)
{
	Instance instance = Instances[PrimitiveIndex()];

	if (payload.refl_count < MaxReflCount)
	{
		float3 origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
		float3 traceDir;
		bool nextTrace = true;
		float3 attenuation;

		[branch]
		if (instance.material == 0)
		{
			// Lambert
			uint i = cbScene.loopCount % MaxSample;
			float2 uv = Hammersley2D(i, MaxSample);
			float3 localDir = HemisphereSampleUniform(uv.x, uv.y);
			float4 qRot = QuatFromTwoVector(float3(0, 0, 1), attr.normal);
			traceDir = QuatRotVector(localDir, qRot);

			attenuation = instance.color.rgb;
		}
		else if (instance.material == 1)
		{
			// Metal
			float3 reflected = Reflect(WorldRayDirection(), attr.normal);
			uint i = cbScene.loopCount % MaxSample;
			float2 uv = Hammersley2D(i, MaxSample);
			float3 localDir = HemisphereSampleCos(uv.x * instance.color.a, uv.y);
			float4 qRot = QuatFromTwoVector(float3(0, 0, 1), reflected);
			traceDir = QuatRotVector(localDir, qRot);

			attenuation = instance.color.rgb;
			nextTrace = (dot(traceDir, attr.normal) > 0.0);
		}
		else
		{
			// Dielectric
			float3 rInDir = WorldRayDirection();
			float refIndex = instance.color.r;
			float3 reflected = Reflect(rInDir, attr.normal);
			float3 outwardNormal;
			float ni_over_nt;
			float3 refracted;
			float reflect_prob;
			float cosine;

			attenuation = (1.0).xxx;
			if (dot(rInDir, attr.normal) > 0.0)
			{
				outwardNormal = -attr.normal;
				ni_over_nt = refIndex;
				cosine = refIndex * dot(rInDir, attr.normal);
			}
			else
			{
				outwardNormal = attr.normal;
				ni_over_nt = 1.0 / refIndex;
				cosine = -dot(rInDir, attr.normal);
			}
			if (Refract(rInDir, outwardNormal, ni_over_nt, refracted))
			{
				// 屈折する場合はSchlickの近似式からフレネルの強さを求める
				reflect_prob = Schlick(cosine, refIndex);
			}
			else
			{
				// 屈折しない場合は全反射
				reflect_prob = 1.0;
			}
			// 乱数を用いて反射か屈折かを選ぶ
			// NOTE: フレネルの強さが強いほど反射が選ばれやすい
			if (GetRandom() < reflect_prob)
			{
				traceDir = reflected;
			}
			else
			{
				traceDir = refracted;
			}
		}

		if (nextTrace)
		{
			// 反射ベクトルに対してレイトレ
			RayDesc ray = { origin, 1e-2, traceDir, TMax };
			HitData refl_payload = { float4(0, 0, 0, 0), payload.refl_count + 1 };
			TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, refl_payload);

			// 結果にインスタンスのカラーを乗算するだけ
			payload.color.rgb = refl_payload.color.rgb * attenuation;
		}
		else
		{
			payload.color.rgb = 0.0;
		}
	}
	else
	{
		// これ以上レイトレしないので黒を返す
		payload.color.rgb = instance.color.rgb;
	}
}

[shader("miss")]
void MissSkyColorProc(inout HitData payload : SV_RayPayload)
{
	float3 col = SkyColor(WorldRayDirection());
	payload.color = float4(col, 1.0);
}

// EOF
