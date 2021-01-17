#include "common.hlsli"
#include "payload.hlsli"

#define TMax			10000.0

struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
	float4	tangent		: TANGENT;
	float2	uv			: TEXCOORD0;
	float3	posWS		: POS_WS;
	float3	viewDir		: VIEDIR;
};

ConstantBuffer<SceneCB>			cbScene			: register(b0);
ConstantBuffer<LightCB>			cbLight			: register(b1);
ConstantBuffer<MaterialCB>		cbMaterial		: register(b2);
ConstantBuffer<TranslucentCB>	cbTrans			: register(b3);

Texture2D		texNormal	: register(t0);
Texture2D		texScreen	: register(t1);
SamplerState	texColor_s	: register(s0);
SamplerState	texScreen_s	: register(s1);

RaytracingAccelerationStructure		TLAS				: register(t2);
StructuredBuffer<MaterialInfo>		rMaterialInfo		: register(t3);
StructuredBuffer<PointLightPos>		rLightPosBuffer		: register(t4);
StructuredBuffer<PointLightColor>	rLightColorBuffer	: register(t5);

ByteAddressBuffer			rIndexBuffers[]		: register(t0, space1);
StructuredBuffer<float3>	rNormalBuffers[]	: register(t0, space2);
StructuredBuffer<float2>	rTexcoordBuffers[]	: register(t0, space3);
Texture2D					texBindless[]		: register(t0, space4);

void GetPrimitiveNormalAndUV(in uint submeshIdx, in uint primIdx, in float2 barycentrics, out float3 normal, out float2 uv)
{
	uint3 indices = Get32bitIndices(primIdx, rIndexBuffers[submeshIdx]);

	float3 ns[3] = {
		rNormalBuffers[submeshIdx][indices.x],
		rNormalBuffers[submeshIdx][indices.y],
		rNormalBuffers[submeshIdx][indices.z]
	};
	normal = ns[0] +
		barycentrics.x * (ns[1] - ns[0]) +
		barycentrics.y * (ns[2] - ns[0]);
	normal = normalize(normal);

	float2 uvs[3] = {
		rTexcoordBuffers[submeshIdx][indices.x],
		rTexcoordBuffers[submeshIdx][indices.y],
		rTexcoordBuffers[submeshIdx][indices.z]
	};
	uv = uvs[0] +
		barycentrics.x * (uvs[1] - uvs[0]) +
		barycentrics.y * (uvs[2] - uvs[0]);
}

float PointLightAttenuation(float d, float r)
{
	const float SourceRadius = 0.01;
	float dd = d * d;
	float rr = SourceRadius * SourceRadius;
	float attn = 2.0 / (dd + rr + d * sqrt(dd + rr));
	float s = 1.0 - smoothstep(r * 0.9, r, d);
	return attn * s;
}

float4 main(PSInput In) : SV_TARGET0
{
	uint2 pixel = uint2(In.position.xy);
	float2 screen_uv = In.position.xy / cbScene.screenInfo.zw;

	float3 normalInTS = texNormal.Sample(texColor_s, In.uv).xyz * 2 - 1;

	float3 T, B, N;
	GetTangentSpace(In.normal, In.tangent, T, B, N);
	float3 normalInWS = normalize(lerp(In.normal, ConvertVectorTangetToWorld(normalInTS, T, B, N), cbTrans.normalIntensity));

	// ray query.
	RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;
	float3 rayDir = normalize(reflect(In.viewDir, normalInWS));
	float3 rayOrigin = In.posWS + N * 1e-3f;
	RayDesc ray = { rayOrigin, 0.0, rayDir, TMax };
	rq.TraceRayInline(TLAS, RAY_FLAG_NONE, ~0, ray);

	// refraction.
	float3 normalInVS = normalize(mul((float3x3)cbScene.mtxWorldToView, normalInWS));
	float refr_int = (1.0 - abs(dot(normalInWS, -In.viewDir))) * cbTrans.refract;
	float2 uv_offset = normalInVS.xy * refr_int * float2(1, -1);
	float2 refr_uv = saturate(screen_uv + uv_offset);
	float3 refr_color = texScreen.SampleLevel(texScreen_s, refr_uv, 0).rgb * float3(1.0, 0.5, 0.5);

	// reflection.
	float3 refl_color = float3(0, 0, 0.5);
	while (rq.Proceed())
	{
		if (rq.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
		{
			// alpha test.
			uint submeshIdx = (rq.CandidateInstanceContributionToHitGroupIndex() / kGeometricContributionMult) + rq.CandidateGeometryIndex();
			uint primIdx = rq.CandidatePrimitiveIndex();
			float2 barycentrics = rq.CandidateTriangleBarycentrics();
			float3 hitN;
			float2 hitUV;
			GetPrimitiveNormalAndUV(NonUniformResourceIndex(submeshIdx), primIdx, barycentrics, hitN, hitUV);

			MaterialInfo info = rMaterialInfo[submeshIdx];
			float alpha = texBindless[NonUniformResourceIndex(info.baseColorIndex)].SampleLevel(texColor_s, hitUV, 2.0).a;

			if (alpha > 0.33)
			{
				rq.CommitNonOpaqueTriangleHit();
			}
		}
	}
	if (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
	{
		// get mesh information.
		uint submeshIdx = (rq.CommittedInstanceContributionToHitGroupIndex() / kGeometricContributionMult) + rq.CommittedGeometryIndex();
		uint primIdx = rq.CommittedPrimitiveIndex();
		float2 barycentrics = rq.CommittedTriangleBarycentrics();
		float3 hitN;
		float2 hitUV;
		GetPrimitiveNormalAndUV(NonUniformResourceIndex(submeshIdx), primIdx, barycentrics, hitN, hitUV);
		hitN = normalize(mul((float3x3)rq.CommittedObjectToWorld3x4(), hitN));

		// shadow ray query.
		RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> occq;
		float3 occRayDir = -cbLight.lightDir.xyz;
		float3 occRayOrigin = rayOrigin + rayDir * rq.CommittedRayT();
		RayDesc occRay = { occRayOrigin + hitN * 1e-4, 0.0, occRayDir, TMax };
		occq.TraceRayInline(TLAS, RAY_FLAG_NONE, ~0, occRay);
		occq.Proceed();

		// lighting.
		MaterialInfo info = rMaterialInfo[submeshIdx];
		float4 baseColor = texBindless[NonUniformResourceIndex(info.baseColorIndex)].SampleLevel(texColor_s, hitUV, 2.0);
		float4 orm = texBindless[NonUniformResourceIndex(info.ormMapIndex)].SampleLevel(texColor_s, hitUV, 2.0);

		float roughness = lerp(cbMaterial.roughnessRange.x, cbMaterial.roughnessRange.y, orm.g) + Epsilon;
		float metallic = lerp(cbMaterial.metallicRange.x, cbMaterial.metallicRange.y, orm.b);
		float3 diffuseColor = baseColor.rgb * (1 - metallic);
		float3 specularColor = 0.04 * (1 - metallic) + baseColor.rgb * metallic;
		float3 directColor = BrdfGGX(diffuseColor, specularColor, roughness, hitN, -cbLight.lightDir.xyz, -rayDir) * cbLight.lightColor.rgb;
		float directShadow = occq.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
		refl_color = directColor * directShadow;

		for (uint plIdx = 0; plIdx < 128; plIdx++)
		{
			PointLightPos lightPos = rLightPosBuffer[plIdx];
			float3 lightDir = lightPos.posAndRadius.xyz - occRayOrigin;
			float lightLen = length(lightDir);
			if (lightLen > lightPos.posAndRadius.w)
				continue;

			PointLightColor lightColor = rLightColorBuffer[plIdx];
			lightDir *= 1.0 / (lightLen + Epsilon);

			directColor = BrdfGGX(diffuseColor, specularColor, roughness, hitN, lightDir, -rayDir)
				* lightColor.color.rgb
				* PointLightAttenuation(lightLen, lightPos.posAndRadius.w);
			refl_color += directColor;
		}
	}

	float3 finalColor = lerp(refr_color, refl_color, cbTrans.opacity);

	return float4(finalColor, 1);
}
