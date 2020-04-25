#include "common.hlsli"
#include "payload.hlsli"
#include "raytrace.hlsli"

#define TMax			10000.0

// global
RaytracingAccelerationStructure			Scene			: register(t0, space0);
Texture2D								WorldNormalTex	: register(t1);
Texture2D								MotionRMTex		: register(t2);
Texture2D<float>						DepthTex		: register(t3);
Texture2D								SkyIrrTex		: register(t4);
Texture2D								SkyHdrTex		: register(t5);
Texture2D<float>						BlueNoise		: register(t6);
RWTexture2D<float4>						RenderTarget	: register(u0);
ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);
ConstantBuffer<ReflectionCB>			cbReflection	: register(b2);
SamplerState							SkyHdrTex_s		: register(s0);

[shader("raygeneration")]
void RTReflectionRGS()
{
	// get parameters from gbuffers.
	uint2 index = DispatchRaysIndex().xy;
	float depth = DepthTex[index].r;
	if (depth >= 1.0)
	{
		RenderTarget[index] = 0;
		return;
	}
	float3 normal = WorldNormalTex[index].xyz * 2.0 - 1.0;
	float4 motionRM = MotionRMTex[index];
	if (motionRM.b - motionRM.a > cbReflection.roughnessMax)
	{
		RenderTarget[index] = 0;
		return;
	}

	// pixel center position to clip space position.
	float2 xy = (float2)index + 0.5;
	float2 clipSpacePos = xy / float2(DispatchRaysDimensions().xy) * float2(2, -2) + float2(-1, 1);

	// clip space position to world space position.
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;
	float3 origin = worldPos.xyz + normal * 1e-2;

	// get reflection vector.
	uint2 noiseUV = index % cbReflection.noiseTexWidth;
	uint count = (uint(BlueNoise[noiseUV] * (float)cbReflection.timeMax) + cbReflection.time) % cbReflection.timeMax;
	float2 Xi = Hammersley2D(count, cbReflection.timeMax);
	float3 viewDir = normalize(worldPos.xyz - cbScene.camPos.xyz);
	float3x3 onb = CalcONB(normal);
	float roughness = motionRM.b + Epsilon;
	float3 halfVector = mul(SampleGgxVndf(mul(onb, -viewDir), roughness, Xi), onb);
	float3 reflDir = reflect(viewDir, halfVector);

	uint ray_flags = RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
	RayDesc ray = { origin, 0.0, reflDir, TMax };
	MaterialPayload payload = (MaterialPayload)0;
	payload.hitT = -1.0;
	TraceRay(Scene, ray_flags, ~0, kMaterialContribution, kGeometricContributionMult, 0, ray, payload);

	// get material parameters.
	MaterialParam mat_param;
	DecodeMaterialPayload(payload, mat_param);

	float3 finalColor;
	[branch]
	if (payload.hitT >= 0.0)
	{
		// prepare shadow ray.
		float3 shadow_dir = -cbLight.lightDir.xyz;
		float3 shadow_pos = origin + reflDir * payload.hitT;
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
		float3 diffuseColor = mat_param.baseColor.rgb * (1 - mat_param.metallic);
		float3 specularColor = 0.04 * (1 - mat_param.metallic) + mat_param.baseColor.rgb * mat_param.metallic;
		float3 directColor = BrdfGGX(diffuseColor, specularColor, mat_param.roughness, N, L, reflDir) * cbLight.lightColor.rgb;
		float3 skyColor = SkyTextureLookup(SkyIrrTex, SkyHdrTex_s, N) * cbLight.skyPower * diffuseColor;
		finalColor = directColor * shadow_factor + skyColor;
	}
	else
	{
		finalColor = SkyTextureLookup(SkyHdrTex, SkyHdrTex_s, reflDir) * cbLight.skyPower;
	}

	float blendRange = cbReflection.roughnessMax * 0.9;
	finalColor *= smoothstep(cbReflection.roughnessMax, blendRange, motionRM.b - motionRM.a);

	// output result.
	RenderTarget[index] = float4(finalColor, 1);
}

[shader("miss")]
void RTReflectionMS(inout MaterialPayload payload : SV_RayPayload)
{
	// not implement.
}

[shader("miss")]
void DirectShadowMS(inout HitPayload payload : SV_RayPayload)
{
	// not implement.
}

// EOF
