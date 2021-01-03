#include "common.hlsli"

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);
ConstantBuffer<MaterialCB>				cbMaterial		: register(b2);

Texture2D							texDtBuffer0		: register(t0);
Texture2D<float2>					texDtBuffer1		: register(t1);
Texture2D							texDtBuffer2		: register(t2);
Texture2D<uint>						texDtBuffer3		: register(t3);
Texture2D							texDtBuffer4		: register(t4);
Texture2D<float>					texDepth			: register(t5);
Texture2D<float>					texScreenShadow		: register(t6);
StructuredBuffer<MaterialInfo>		rMaterialInfo		: register(t7);
StructuredBuffer<PointLightPos>		rLightPosBuffer		: register(t8);
StructuredBuffer<PointLightColor>	rLightColorBuffer	: register(t9);
StructuredBuffer<ClusterInfo>		rClusterInfo		: register(t10);

Texture2D							texBindless[]		: register(t0, space1);

SamplerState						samTexture			: register(s0);

RWTexture2D<float4>	rwOutput		: register(u0);

ClusterInfo GetCluster(float2 screenPos, float depth)
{
	float nearZ = cbScene.screenInfo.x;
	float farZ = cbScene.screenInfo.y;
	float viewDepth = ToViewDepth(depth, nearZ, farZ);
	uint z = uint(max(-viewDepth - nearZ, 0) * (float)CLUSTER_DIV_Z / (farZ - nearZ));
	uint2 xy = uint2(screenPos * (float)CLUSTER_DIV_XY);

	uint infoIndex = z * CLUSTER_DIV_XY * CLUSTER_DIV_XY + xy.y * CLUSTER_DIV_XY + xy.x;

	return rClusterInfo[infoIndex];
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

float3 Lighting(uint2 pixelPos, float depth)
{
	// get dtbuffer.
	float4 worldQuat = UnpackQuat(texDtBuffer0[pixelPos]);
	float2 uv = texDtBuffer1[pixelPos];
	float4 derivativeUV = texDtBuffer2[pixelPos];
	uint matId = texDtBuffer3[pixelPos];
	bool binormalSign = (matId & 0x8000) != 0;
	matId = matId & 0x7fff;
	float4 meshletColor = texDtBuffer4[pixelPos];

	// sample textures.
	MaterialInfo info = rMaterialInfo[matId];
	float4 baseColor = texBindless[NonUniformResourceIndex(info.baseColorIndex)].SampleGrad(samTexture, uv, derivativeUV.xy, derivativeUV.zw);
	float3 normalInTS = texBindless[NonUniformResourceIndex(info.normalMapIndex)].SampleGrad(samTexture, uv, derivativeUV.xy, derivativeUV.zw).rgb * 2 - 1;
	float4 orm = texBindless[NonUniformResourceIndex(info.ormMapIndex)].SampleGrad(samTexture, uv, derivativeUV.xy, derivativeUV.zw);

	normalInTS *= (binormalSign ? float3(1, -1, 1) : 1);

	float3 normalInWS = ConvertVectorTangentToWorld(normalInTS, worldQuat);
	float roughness = lerp(cbMaterial.roughnessRange.x, cbMaterial.roughnessRange.y, orm.g) + Epsilon;
	float metallic = lerp(cbMaterial.metallicRange.x, cbMaterial.metallicRange.y, orm.b);

	// get world position.
	float2 screenPos = ((float2)pixelPos + 0.5) / cbScene.screenInfo.zw;
	float2 clipSpacePos = screenPos * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	float3 viewDirInWS = normalize(worldPos.xyz - cbScene.camPos.xyz);
	float3 diffuseColor = baseColor.rgb * (1 - metallic);
	float3 specularColor = 0.04 * (1 - metallic) + baseColor.rgb * metallic;

	// directional light.
	float3 directColor = BrdfGGX(diffuseColor, specularColor, roughness, normalInWS, -cbLight.lightDir.rgb, -viewDirInWS) * cbLight.lightColor.rgb;
	float directShadow = texScreenShadow[pixelPos];
	float3 finalColor = directColor * directShadow;

	// point lights.
	ClusterInfo cluster = GetCluster(screenPos, depth);
	for (uint flagIndex = 0; flagIndex < 4; flagIndex++)
	{
		uint flags = cluster.lightFlags[flagIndex];
		uint baseIndex = flagIndex * 32;
		while (flags)
		{
			uint index = firstbitlow(flags);
			flags &= ~(0x1 << index);

			PointLightPos lightPos = rLightPosBuffer[baseIndex + index];
			PointLightColor lightColor = rLightColorBuffer[baseIndex + index];

			float3 lightDir = lightPos.posAndRadius.xyz - worldPos.xyz;
			float lightLen = length(lightDir);
			lightDir *= 1.0 / (lightLen + Epsilon);

			directColor = BrdfGGX(diffuseColor, specularColor, roughness, normalInWS, lightDir, -viewDirInWS)
				* lightColor.color.rgb
				* PointLightAttenuation(lightLen, lightPos.posAndRadius.w);
			finalColor += directColor;
		}
	}

	return finalColor * meshletColor.rgb;
}

[numthreads(8, 8, 1)]
void main(
	uint3 gid : SV_GroupID,
	uint3 gtid : SV_GroupThreadID,
	uint3 did : SV_DispatchThreadID)
{
	uint2 pixelPos = did.xy;

	if (all(pixelPos < (uint2)cbScene.screenInfo.zw))
	{
		float depth = texDepth[pixelPos];
		[branch]
		if (depth >= 1.0)
		{
			rwOutput[pixelPos] = float4(0, 0, 1, 1);
		}
		else
		{
			rwOutput[pixelPos] = float4(Lighting(pixelPos, depth), 1);
		}
	}
}