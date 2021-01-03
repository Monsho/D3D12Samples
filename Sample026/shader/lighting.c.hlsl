#include "common.hlsli"

ConstantBuffer<SceneCB>					cbScene			: register(b0);
ConstantBuffer<LightCB>					cbLight			: register(b1);

Texture2D							texGBuffer0			: register(t0);
Texture2D							texGBuffer1			: register(t1);
Texture2D							texGBuffer2			: register(t2);
Texture2D							texGBuffer3			: register(t3);
Texture2D<float>					texDepth			: register(t4);
Texture2D<float>					texScreenShadow		: register(t5);
StructuredBuffer<PointLightPos>		rLightPosBuffer		: register(t6);
StructuredBuffer<PointLightColor>	rLightColorBuffer	: register(t7);
StructuredBuffer<ClusterInfo>		rClusterInfo		: register(t8);

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
	// get gbuffer.
	float4 gin0 = texGBuffer0[pixelPos];
	float4 gin1 = texGBuffer1[pixelPos];
	float4 gin2 = texGBuffer2[pixelPos];
	float4 gin3 = texGBuffer3[pixelPos];
	GBuffer gb = DecodeGBuffer(gin0, gin1, gin2);

	// get world position.
	float2 screenPos = ((float2)pixelPos + 0.5) / cbScene.screenInfo.zw;
	float2 clipSpacePos = screenPos * float2(2, -2) + float2(-1, 1);
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, depth, 1));
	worldPos.xyz /= worldPos.w;

	float3 viewDirInWS = normalize(worldPos.xyz - cbScene.camPos.xyz);
	float3 normalInWS = ConvertVectorTangentToWorld(float3(0, 0, 1), gb.worldQuat);
	float3 diffuseColor = gb.baseColor.rgb * (1 - gb.metallic);
	float3 specularColor = 0.04 * (1 - gb.metallic) + gb.baseColor.rgb * gb.metallic;

	// directional light.
	float3 directColor = BrdfGGX(diffuseColor, specularColor, gb.roughness, normalInWS, -cbLight.lightDir.rgb, -viewDirInWS) * cbLight.lightColor.rgb;
	float directShadow = texScreenShadow[pixelPos];
	float3 finalColor = directColor * directShadow;

	// point lights.
	ClusterInfo cluster = GetCluster(screenPos, depth);
	for (uint flagIndex = 0; flagIndex < 4; flagIndex++)
	{
		uint flags = cluster.lightFlags[flagIndex];
		uint baseIndex = flagIndex * 32;
#if 0
		for (uint index = 0; index < 32; index++)
		{
#else
		while (flags)
		{
			uint index = firstbitlow(flags);
			flags &= ~(0x1 << index);
#endif

			PointLightPos lightPos = rLightPosBuffer[baseIndex + index];
			PointLightColor lightColor = rLightColorBuffer[baseIndex + index];

			float3 lightDir = lightPos.posAndRadius.xyz - worldPos.xyz;
			float lightLen = length(lightDir);
			lightDir *= 1.0 / (lightLen + Epsilon);

			directColor = BrdfGGX(diffuseColor, specularColor, gb.roughness, normalInWS, lightDir, -viewDirInWS)
				* lightColor.color.rgb
				* PointLightAttenuation(lightLen, lightPos.posAndRadius.w);
			finalColor += directColor;
		}
	}

	return finalColor * gin3.rgb;
}

[numthreads(8, 8, 1)]
void main(
	uint3 gid : SV_GroupID,
	uint3 gtid : SV_GroupThreadID,
	uint3 did : SV_DispatchThreadID)
{
	//uint2 pixelPos = gid.xy * uint2(8, 8) + gtid.xy;
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

	//pixelPos = pixelPos + uint2(0, 4);
	////uint2 pixelPos = did.xy;

	//if (all(pixelPos < (uint2)cbScene.screenInfo.zw))
	//{
	//	float depth = texDepth[pixelPos];
	//	[branch]
	//	if (depth >= 1.0)
	//	{
	//		rwOutput[pixelPos] = float4(0, 0, 1, 1);
	//	}
	//	else
	//	{
	//		rwOutput[pixelPos] = float4(Lighting(pixelPos, depth), 1);
	//	}
	//}
}