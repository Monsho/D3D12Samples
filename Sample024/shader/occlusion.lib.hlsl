#include "common.hlsli"
#include "payload.hlsli"

// local
ByteAddressBuffer					Indices			: register(t9);
StructuredBuffer<float3>			VertexNormal	: register(t10);
StructuredBuffer<float2>			VertexUV		: register(t11);
Texture2D							texBaseColor	: register(t12);
SamplerState						texBaseColor_s	: register(s4);

[shader("closesthit")]
void OcclusionCHS(inout HitPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
	payload.hitT = RayTCurrent();
}

[shader("anyhit")]
void OcclusionAHS(inout HitPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
	uint3 indices = Get32bitIndices(PrimitiveIndex(), Indices);

	float2 uvs[3] = {
		VertexUV[indices.x],
		VertexUV[indices.y],
		VertexUV[indices.z]
	};
	float2 uv = uvs[0] +
		attr.barycentrics.x * (uvs[1] - uvs[0]) +
		attr.barycentrics.y * (uvs[2] - uvs[0]);

	float opacity = texBaseColor.SampleLevel(texBaseColor_s, uv, 0.0).a;
	if (opacity < 0.33)
	{
		IgnoreHit();
	}
}


// EOF
