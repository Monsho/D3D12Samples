#ifndef MATH_HLSLI
#define MATH_HLSLI


float ToLinearDepth(float perspDepth, float n, float f)
{
	return -n / ((n - f) * perspDepth + f);
}

float ToViewDepth(float perspDepth, float m33, float m43, float m34)
{
	return -m43 / (m33 - m34 * perspDepth);
}

float2 VectorToRadialCoords(in float3 vc)
{
	float3 normalizedCoords = normalize(vc);
	float latitude = acos(normalizedCoords.y);
	float longitude = atan2(normalizedCoords.z, normalizedCoords.x);
	float2 uv = {
		0.5 - (longitude * 0.5 / PI),
		(latitude / PI)
	};
	return uv;
}

void GetTangentSpace(in float3 normal, in float4 tangent, out float3 o_tangent, out float3 o_binormal, out float3 o_normal)
{
	o_normal = normalize(normal);
	o_tangent = normalize(tangent.xyz);
	o_binormal = normalize(cross(o_normal, o_tangent));
	o_tangent = cross(o_binormal, o_normal);
	o_binormal *= -sign(tangent.w);
}

float4 TangentSpaceToQuat(in float3 T, in float3 B, in float3 N)
{
	float4 q;
	float trace = T.x + B.y + N.z;
	if (trace > 0)
	{
		float s = 0.5 / sqrt(trace + 1.0);
		q.w = 0.25 / s;
		q.x = (B.z - N.y) * s;
		q.y = (N.x - T.z) * s;
		q.z = (T.y - B.x) * s;
	}
	else
	{
		if (T.x > B.y && T.x > N.z)
		{
			float s = 2.0 * sqrt(1.0 + T.x - B.y - N.z);
			q.w = (B.z - N.y) / s;
			q.x = 0.25 * s;
			q.y = (B.x + T.y) / s;
			q.z = (N.x + T.z) / s;
		}
		else if (B.y > N.z)
		{
			float s = 2.0 * sqrt(1.0 + B.y - T.x - N.z);
			q.w = (N.x - T.z) / s;
			q.x = (B.x + T.y) / s;
			q.y = 0.25 * s;
			q.z = (N.y + B.z) / s;
		}
		else
		{
			float s = 2.0 * sqrt(1.0 + N.z - T.x - B.y);
			q.w = (T.y - B.x) / s;
			q.x = (N.x + T.z) / s;
			q.y = (N.y + B.z) / s;
			q.z = 0.25 * s;
		}
	}
	return q;
}

float3 ConvertVectorTangetToWorld(in float3 v, in float3 T, in float3 B, in float3 N)
{
	float3 ret = v.x * T + v.y * B + v.z * N;
	return ret;
}

float3 ConvertVectorTangentToWorld(in float3 v, in float4 quat)
{
	float3 t = 2 * cross(quat.xyz, v);
	return v + quat.w * t + cross(quat.xyz, t);
}
float3 ConvertVectorWorldToTangent(in float3 v, in float4 quat)
{
	float3 u = -quat.xyz;
	float3 t = 2 * cross(u, v);
	return v + quat.w * t + cross(u, t);
}

float4 PackQuat(in float4 quat)
{
	float4 absQ = abs(quat);
	float absMaxComponent = max(max(absQ.x, absQ.y), max(absQ.z, absQ.w));

	uint maxCompIdx = 0;
	float maxComponent = quat.x;

	[unroll]
	for (uint i = 0; i < 4; ++i)
	{
		if (absQ[i] == absMaxComponent)
		{
			maxCompIdx = i;
			maxComponent = quat[i];
		}
	}

	if (maxComponent < 0.0)
		quat *= -1.0;

	float3 components;
	if (maxCompIdx == 0)
		components = quat.yzw;
	else if (maxCompIdx == 1)
		components = quat.xzw;
	else if (maxCompIdx == 2)
		components = quat.xyw;
	else
		components = quat.xyz;

	const float maxRange = 1.0 / sqrt(2.0);
	components /= maxRange;
	components = components * 0.5 + 0.5;

	return float4(components, maxCompIdx / 3.0);
}

float4 UnpackQuat(in float4 packed)
{
	uint maxCompIdx = uint(packed.w * 3.0);
	packed.xyz = packed.xyz * 2.0 - 1.0;
	const float maxRange = 1.0 / sqrt(2.0);
	packed.xyz *= maxRange;
	float maxComponent = sqrt(1.0 - saturate(packed.x * packed.x + packed.y * packed.y + packed.z * packed.z));

	float4 q;
	if (maxCompIdx == 0)
		q = float4(maxComponent, packed.xyz);
	else if (maxCompIdx == 1)
		q = float4(packed.x, maxComponent, packed.yz);
	else if (maxCompIdx == 2)
		q = float4(packed.xy, maxComponent, packed.z);
	else
		q = float4(packed.xyz, maxComponent);

	return q;
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
float2 UnitVectorToOctahedron(float3 n)
{
	n /= dot(1.0, abs(n));
	if (n.z < 0.0)
	{
		n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
	}
	n.xy = n.xy * 0.5 + 0.5;
	return n.xy;
}
float3 OctahedronToUnitVector(float2 f)
{
	f = f * 2.0 - 1.0;

	// https://twitter.com/Stubbesaurus/status/937994790553227264
	float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
	float t = saturate(-n.z);
	n.xy += n.xy >= 0.0 ? -t : t;
	return normalize(n);
}

uint MortonCode2(uint x)
{
	x &= 0x0000ffff;
	x = (x ^ (x << 8)) & 0x00ff00ff;
	x = (x ^ (x << 4)) & 0x0f0f0f0f;
	x = (x ^ (x << 2)) & 0x33333333;
	x = (x ^ (x << 1)) & 0x55555555;
	return x;
}

uint MortonCodeInv2(uint x)
{
	x &= 0x55555555;
	x = (x ^ (x >>  1)) & 0x33333333;
	x = (x ^ (x >>  2)) & 0x0f0f0f0f;
	x = (x ^ (x >>  4)) & 0x00ff00ff;
	x = (x ^ (x >>  8)) & 0x0000ffff;
	return x;
}

uint MortonEncode2D(uint2 Pixel)
{
	return MortonCode2(Pixel.x) | (MortonCode2(Pixel.y) << 1);
}

uint2 MortonDecode2D(uint code)
{
	return uint2(MortonCodeInv2(code >> 0), MortonCodeInv2(code >> 1));
}


#endif
