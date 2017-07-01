#define VERTEX_STRIDE		(4 * 3 + 4 * 3)
#define NORMAL_OFFSET		(4 * 3)

cbuffer cbWorld			: register(b0)
{
	float4x4	mtxWorld;
	uint		vertexCount;
}

ByteAddressBuffer		rSrcVBuffer		: register(t0);
RWByteAddressBuffer		rwDstVBuffer	: register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint vertexIndex = dispatchID.x;
	if (vertexIndex >= vertexCount)
	{
		return;
	}

	// í∏ì_ç¿ïW
	uint baseOffset = VERTEX_STRIDE * vertexIndex;
	uint3 uPos = rSrcVBuffer.Load3(baseOffset);
	float3 pos = asfloat(uPos);
	pos = mul(mtxWorld, float4(pos, 1.0)).xyz;
	rwDstVBuffer.Store3(baseOffset, asuint(pos));

	// ñ@ê¸
	uint normalOffset = baseOffset + NORMAL_OFFSET;
	uint3 uNormal = rSrcVBuffer.Load3(normalOffset);
	float3 normal = asfloat(uNormal);
	normal = mul((float3x3)mtxWorld, normal).xyz;
	rwDstVBuffer.Store3(normalOffset, asuint(normal));
}

//	EOF
