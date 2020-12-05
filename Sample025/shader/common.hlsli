#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include "constant.h"

#define LANE_COUNT_IN_WAVE	32

float3 SkyColor(float3 w_dir)
{
	float3 t = w_dir.y * 0.5 + 0.5;
	return saturate((1.0).xxx * (1.0 - t) + float3(0.5, 0.7, 1.0) * t);
}

float ToLinearDepth(float perspDepth, float n, float f)
{
	return -n / ((n - f) * perspDepth + f);
}

float ToViewDepth(float perspDepth, float n, float f)
{
	return -n * f / ((n - f) * perspDepth + f);
}

float4 RotateGridSuperSample(Texture2D tex, SamplerState sam, float2 uv, float bias)
{
	float2 dx = ddx(uv);
	float2 dy = ddy(uv);

	float2 uvOffsets = float2(0.125, 0.375);
	float2 offsetUV = float2(0.0, 0.0);

	float4 col = 0;
	offsetUV = uv + uvOffsets.x * dx + uvOffsets.y * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	offsetUV = uv - uvOffsets.x * dx - uvOffsets.y * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	offsetUV = uv + uvOffsets.y * dx - uvOffsets.x * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	offsetUV = uv - uvOffsets.y * dx + uvOffsets.x * dy;
	col += tex.SampleBias(sam, offsetUV, bias);
	col *= 0.25;

	return col;
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
	o_binormal = normalize(cross(o_tangent, o_normal));
	o_tangent = cross(o_normal, o_binormal);
	o_binormal *= tangent.w;
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

float3 DiffuseLambert(in float3 diffuseColor)
{
	return diffuseColor / PI;
}

float SpecularDGGX(in float NoH, in float roughness)
{
	float NoH2 = NoH * NoH;
	float a2 = NoH2 * roughness * roughness;
	float k = roughness / (1 - NoH2 + a2);
	return k * k / PI;
}

float SpecularGSmithCorrelated(in float NoV, in float NoL, in float roughness)
{
	float a2 = roughness * roughness;
	float V = NoL * sqrt(NoV * NoV * (1 - a2) + a2);
	float L = NoV * sqrt(NoL * NoL * (1 - a2) + a2);
	return 0.5 / (V + L);
}

float3 SpecularFSchlick(in float VoH, in float3 f0)
{
	float f = pow(1 - VoH, 5);
	return f + f0 * (1 - f);
}

float3 BrdfGGX(in float3 diffuseColor, in float3 specularColor, in float linearRoughness, in float3 N, in float3 L, in float3 V)
{
	float3 H = normalize(L + V);

	float NoV = abs(dot(N, V)) + Epsilon;
	float NoL = saturate(dot(N, L));
	float NoH = saturate(dot(N, H));
	float LoH = saturate(dot(L, H));

	float roughness = linearRoughness * linearRoughness;

	float D = SpecularDGGX(NoH, roughness);
	float G = SpecularGSmithCorrelated(NoV, NoL, roughness);
	float3 F = SpecularFSchlick(LoH, specularColor);

	float3 SResult = D * G * F;
	float3 DResult = DiffuseLambert(diffuseColor);

	return (DResult + SResult) * NoL;
}


// get triangle indices functions.
uint3 GetTriangleIndices2byte(uint offset, ByteAddressBuffer indexBuffer)
{
	uint alignedOffset = offset & ~0x3;
	uint2 indices4 = indexBuffer.Load2(alignedOffset);

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

uint3 Get16bitIndices(uint primIdx, ByteAddressBuffer indexBuffer)
{
	uint indexOffset = primIdx * 2 * 3;
	return GetTriangleIndices2byte(indexOffset, indexBuffer);
}

uint3 Get32bitIndices(uint primIdx, ByteAddressBuffer indexBuffer)
{
	uint indexOffset = primIdx * 4 * 3;
	uint3 ret = indexBuffer.Load3(indexOffset);
	return ret;
}

struct GBuffer
{
	float4	worldQuat;
	float4	baseColor;
	float	roughness;
	float	metallic;
};

void EncodeGBuffer(in GBuffer gb, out float4 o0, out float4 o1, out float4 o2)
{
	o0 = PackQuat(gb.worldQuat);
	o1 = gb.baseColor;
	o2 = float4(gb.roughness, gb.metallic, 0, 0);
}

GBuffer DecodeGBuffer(in float4 i0, in float4 i1, in float4 i2)
{
	GBuffer ret;
	ret.worldQuat = UnpackQuat(i0);
	ret.baseColor = i1;
	ret.roughness = i2.x;
	ret.metallic = i2.y;
	return ret;
}

#define MaxSample			512

#endif // COMMON_HLSLI
//	EOF
