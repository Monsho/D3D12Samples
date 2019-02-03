struct SceneCB
{
	float4x4	mtxProjToWorld;
	float4		camPos;
	float4		lightDir;
	float4		lightColor;
};

struct HitData
{
	float4 color;
};

// global
RaytracingAccelerationStructure		Scene			: register(t0, space0);
RWTexture2D<float4>					RenderTarget	: register(u0);
ConstantBuffer<SceneCB>				cbScene			: register(b0);

// local
Texture2D							texImage		: register(t1);
StructuredBuffer<float2>			VertexUV		: register(t2);
ByteAddressBuffer					Indices			: register(t3);
SamplerState						samImage		: register(s0);


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

[shader("raygeneration")]
void RayGenerator()
{
	// ピクセル中心座標をクリップ空間座標に変換
	uint2 index = DispatchRaysIndex().xy;
	float2 xy = (float2)index + 0.5;
	float2 clipSpacePos = xy / float2(DispatchRaysDimensions().xy) * float2(2, -2) + float2(-1, 1);

	// クリップ空間座標をワールド空間座標に変換
	float4 worldPos = mul(cbScene.mtxProjToWorld, float4(clipSpacePos, 1, 1));

	// ワールド空間座標とカメラ位置からレイを生成
	worldPos.xyz /= worldPos.w;
	float3 origin = cbScene.camPos.xyz;
	float3 direction = normalize(worldPos.xyz - origin);

	// Let's レイトレ！
	RayDesc ray = { origin, 0.0f, direction, 10000.0f };
	HitData payload = { float4(0, 0, 0, 0) };
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

	// Write the raytraced color to the output texture.
	RenderTarget[index] = payload.color;
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

	payload.color = texImage.SampleLevel(samImage, uv, 0.0);
}

[shader("miss")]
void MissProcessor(inout HitData payload : SV_RayPayload)
{
	payload.color = float4(0, 0, 0, 1);
}

// EOF
