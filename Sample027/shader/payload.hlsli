#ifndef PAYLOAD_HLSLI
#define PAYLOAD_HLSLI

#define kGeometricContributionMult		2
#define kMaterialContribution			0
#define kShadowContribution				1

struct HitPayload
{
	float	hitT;
};

struct MaterialPayload
{
	uint2	normalMetallic;
	uint	baseColorUnorm;
	float	hitT;
};

struct MaterialParam
{
	float3	normal;
	float	metallic;
	float4	baseColor;
	float	hitT;
};

void EncodeMaterialPayload(
	in MaterialParam param,
	inout MaterialPayload payload)
{
	payload.hitT = param.hitT;

	uint3 n = uint3(param.normal * 32767.0 + 32767.0);
	uint m = uint(param.metallic * 65535.0);
	payload.normalMetallic.x = (n.x << 16) | (n.y << 0);
	payload.normalMetallic.y = (n.z << 16) | (m << 0);

	uint r = uint(param.baseColor.r * 255.0);
	uint g = uint(param.baseColor.g * 255.0);
	uint b = uint(param.baseColor.b * 255.0);
	payload.baseColorUnorm = (b << 16) | (g << 8) | (r << 0);
}

void DecodeMaterialPayload(
	in MaterialPayload payload,
	inout MaterialParam param)
{
	param.hitT = payload.hitT;

	uint3 n = uint3(
		payload.normalMetallic.x >> 16,
		payload.normalMetallic.x & 0xffff,
		payload.normalMetallic.y >> 16);
	param.normal = (float3(n) - 32767.0) * (1.0 / 32767.0);

	param.metallic = float(payload.normalMetallic.y & 0xffff) / 65535.0;

	uint4 bc = uint4(
		(payload.baseColorUnorm >> 0) & 0xff,
		(payload.baseColorUnorm >> 8) & 0xff,
		(payload.baseColorUnorm >> 16) & 0xff,
		0xff);
	param.baseColor = float4(bc) * (1.0 / 255.0);
}

#endif // PAYLOAD_HLSLI
//	EOF
