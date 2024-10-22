#include "common.hlsli"
#include "payload.hlsli"


// local
ByteAddressBuffer					Indices			: register(t9);
StructuredBuffer<float3>			VertexNormal	: register(t10);
StructuredBuffer<float2>			VertexUV		: register(t11);
Texture2D							texBaseColor	: register(t12);
Texture2D							texORM			: register(t13);
SamplerState						texBaseColor_s	: register(s4);

[shader("closesthit")]
void MaterialCHS(inout MaterialPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
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

	MaterialParam param;
	param.hitT = RayTCurrent();

	param.baseColor = texBaseColor.SampleLevel(texBaseColor_s, uv, 0.0);
	float3 orm = texORM.SampleLevel(texBaseColor_s, uv, 0.0).rgb;
	param.metallic = orm.b;
	param.roughness = orm.g;

	// don't use normal maps.
	float3 n0 = VertexNormal[indices.x];
	param.normal = normalize(n0 +
		attr.barycentrics.x * (VertexNormal[indices.y] - n0) +
		attr.barycentrics.y * (VertexNormal[indices.z] - n0));

	param.flag = 0;
	param.flag |= (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE) ? kFlagBackFaceHit : 0;

	EncodeMaterialPayload(param, payload);
}

[shader("anyhit")]
void MaterialAHS(inout MaterialPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
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
