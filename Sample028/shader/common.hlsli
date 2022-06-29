#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include "constant.h"
#include "math.hlsli"

#define LANE_COUNT_IN_WAVE	32

float3 SkyColor(float3 w_dir)
{
	float3 t = w_dir.y * 0.5 + 0.5;
	return saturate((1.0).xxx * (1.0 - t) + float3(0.5, 0.7, 1.0) * t);
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

	float NoV = saturate(abs(dot(N, V)) + Epsilon);
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
	o2 = float4(lerp(0.01, 1.0, gb.roughness), gb.metallic, 0, 0);
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
