struct Viewport
{
	float left;
	float top;
	float right;
	float bottom;
};

struct RayGenCB
{
	Viewport viewport;
	Viewport stencil;
};

struct HitData
{
	float4 color;
};

RaytracingAccelerationStructure		Scene			: register(t0, space0);
RWTexture2D<float4>					RenderTarget	: register(u0);
ConstantBuffer<RayGenCB>			cbRayGen		: register(b0);

bool IsInsideViewport(float2 p, Viewport viewport)
{
	return (p.x >= viewport.left && p.x <= viewport.right)
		&& (p.y >= viewport.top && p.y <= viewport.bottom);
}


[shader("raygeneration")]
void RayGenerator()
{
	float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

	// 正射影としてレイを飛ばす
	float3 rayDir = float3(0, 0, 1);
	float3 origin = float3(
		lerp(cbRayGen.viewport.left, cbRayGen.viewport.right, lerpValues.x),
		lerp(cbRayGen.viewport.top, cbRayGen.viewport.bottom, lerpValues.y),
		0.0f);

	if (IsInsideViewport(origin.xy, cbRayGen.stencil))
	{
		// 指定Viewport内ならレイを飛ばす
		RayDesc myRay = { origin, 0.0f, rayDir, 10000.0f };
		HitData payload = { float4(0, 0, 0, 0) };
		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, myRay, payload);

		// 結果が返ってくるのでUAVにカラーを描き込む
		RenderTarget[DispatchRaysIndex().xy] = payload.color;
	}
	else
	{
		// 指定領域外なので適当なカラーを出力
		RenderTarget[DispatchRaysIndex().xy] = float4(lerpValues, 0, 1);
	}
}

[shader("closesthit")]
void ClosestHitProcessor(inout HitData payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
	float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	payload.color = float4(barycentrics, 1);
}

[shader("miss")]
void MissProcessor(inout HitData payload : SV_RayPayload)
{
	payload.color = float4(0, 0, 0, 1);
}

// EOF
