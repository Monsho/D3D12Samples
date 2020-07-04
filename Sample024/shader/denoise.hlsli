#ifndef DENOISE_HLSLI
#define DENOISE_HLSLI

#include "common.hlsli"

ConstantBuffer<SceneCB>		cbScene			: register(b0);

Texture2D		texNormal		: register(t0);
Texture2D		texDepth		: register(t1);
Texture2D		texResult		: register(t2);

#ifndef kNormalThreshold
#	define	kNormalThreshold		0.5
#endif
#ifndef kDepthThreshold
#	define	kDepthThreshold			0.00001
#endif
#ifndef kBlurWidth
#	define	kBlurWidth				3
#endif
#ifndef kBlurX
#	define	kBlurX					1
#endif

float CalcWeight(int2 uv, int2 offset, float3 base_normal, float base_depth, float gauss_weight)
{
	float3 n = normalize(texNormal[uv + offset].xyz * 2 - 1);
	float d = texDepth[uv + offset].r;

	float w = gauss_weight;
	w *= smoothstep(kNormalThreshold, 1.0, dot(base_normal, n));	// normal weight
	w *= smoothstep(kDepthThreshold, 0, abs(base_depth - d));		// depth weight
	return w;
}

float4 Denoise(int2 uv, float delta)
{
	float3 base_normal = normalize(texNormal[uv].xyz * 2 - 1);
	float base_depth = texDepth[uv].r;
	float4 result = texResult[uv];
	float total_weight = 1;
	float gd = 1.0 / (2.0 * delta * delta);

	for (int i = 1; i <= kBlurWidth; i++)
	{
		float gauss_weight = exp(-(float)(i * i) * gd);
#if kBlurX
		int2 offset = int2(i, 0);
#else
		int2 offset = int2(0, i);
#endif
		float w = CalcWeight(uv, offset, base_normal, base_depth, gauss_weight);
		float4 r = texResult[uv + offset];
		result.xy += r.xy * w;
		total_weight += w;

		w = CalcWeight(uv, -offset, base_normal, base_depth, gauss_weight);
		r = texResult[uv - offset];
		result.xy += r.xy * w;
		total_weight += w;
	}

	result.xy /= total_weight;
	return result;
}

#endif	// DENOISE_HLSLI
