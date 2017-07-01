#define VERTEX_STRIDE		(4 * 3 + 4 * 2)
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

	// ’¸“_À•W
	uint baseOffset = VERTEX_STRIDE * vertexIndex;
	uint3 uPos = rSrcVBuffer.Load3(baseOffset);
	float3 pos = asfloat(uPos);
	pos = mul(mtxWorld, float4(pos, 1.0)).xyz;
	rwDstVBuffer.Store3(baseOffset, asuint(pos));

	// –@ü
	uint normalOffset = baseOffset + NORMAL_OFFSET;
	uint2 uPackedNormal = rSrcVBuffer.Load2(normalOffset);
	uint3 uNormal = { uPackedNormal.x & 0xffff, (uPackedNormal.x >> 16) & 0xffff, uPackedNormal.y & 0xffff };
	float3 normal = f16tof32(uNormal);
	normal = mul((float3x3)mtxWorld, normal).xyz;
	uNormal = f32tof16(normal);
	uPackedNormal = uint2(uNormal.x | (uNormal.y << 16), uNormal.z);
	rwDstVBuffer.Store2(normalOffset, uPackedNormal);
}

//	EOF
