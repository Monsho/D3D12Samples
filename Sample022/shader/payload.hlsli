#ifndef PAYLOAD_HLSLI
#define PAYLOAD_HLSLI

#define kGeometricContributionMult		2
#define kMaterialContribution			0
#define kShadowContribution				1

#define kFlagBackFaceHit				0x01 << 0

struct HitPayload
{
	float	hitT;
};

struct MaterialPayload
{
	uint2	normalMetallicRoughness;
	uint	baseColorUnorm;
	float	hitT;
};

struct MaterialParam
{
	float3	normal;
	float	metallic;
	float	roughness;
	float4	baseColor;
	float	hitT;
	uint	flag;
};

void EncodeMaterialPayload(
	in MaterialParam param,
	inout MaterialPayload payload)
{
	payload.hitT = param.hitT;

	{
		uint3 n = uint3(param.normal * 32767.0 + 32767.0);
		uint m = uint(param.metallic * 255.0);
		uint r = uint(param.roughness * 255.0);
		uint mr = (m << 8) | (r << 0);
		payload.normalMetallicRoughness.x = (n.x << 16) | (n.y << 0);
		payload.normalMetallicRoughness.y = (n.z << 16) | (mr << 0);
	}

	{
		uint r = uint(param.baseColor.r * 255.0);
		uint g = uint(param.baseColor.g * 255.0);
		uint b = uint(param.baseColor.b * 255.0);
		uint a = param.flag & 0xff;
		payload.baseColorUnorm = (a << 24) | (b << 16) | (g << 8) | (r << 0);
	}
}

void DecodeMaterialPayload(
	in MaterialPayload payload,
	inout MaterialParam param)
{
	param.hitT = payload.hitT;

	uint3 n = uint3(
		payload.normalMetallicRoughness.x >> 16,
		payload.normalMetallicRoughness.x & 0xffff,
		payload.normalMetallicRoughness.y >> 16);
	param.normal = (float3(n) - 32767.0) * (1.0 / 32767.0);

	param.metallic = saturate(float((payload.normalMetallicRoughness.y >> 8) & 0x00ff) / 255.0);
	param.roughness = saturate(float(payload.normalMetallicRoughness.y & 0x00ff) / 255.0);

	uint4 bc = uint4(
		(payload.baseColorUnorm >> 0) & 0xff,
		(payload.baseColorUnorm >> 8) & 0xff,
		(payload.baseColorUnorm >> 16) & 0xff,
		0xff);
	param.baseColor = saturate(float4(bc) * (1.0 / 255.0));
	param.flag = (payload.baseColorUnorm >> 24) & 0xff;
}

#endif // PAYLOAD_HLSLI
//	EOF
