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
	float3	emissive;
	float	pad;
	uint2	normalRoughnessMetallic;		// 16bit unorm normal.xyz + 8bit roughness + 8bit metallic
	uint	baseColorUnorm;
	float	hitT;
};

struct MaterialParam
{
	float3	emissive;
	float3	normal;
	float	roughness;
	float	metallic;
	float4	baseColor;
	float	hitT;
	uint	flag;
};

void EncodeMaterialPayload(
	in MaterialParam param,
	inout MaterialPayload payload)
{
	payload.emissive = param.emissive;
	payload.hitT = param.hitT;

	uint3 n = uint3(param.normal * 32767.0 + 32767.0);
	uint rough = uint(param.roughness * 255.0);
	uint metal = uint(param.metallic * 255.0);
	payload.normalRoughnessMetallic.x = (n.x << 16) | (n.y << 0);
	payload.normalRoughnessMetallic.y = (n.z << 16) | (rough << 8) | (metal << 0);

	uint r = uint(param.baseColor.r * 255.0);
	uint g = uint(param.baseColor.g * 255.0);
	uint b = uint(param.baseColor.b * 255.0);
	uint a = param.flag & 0xff;
	payload.baseColorUnorm = (a << 24) | (b << 16) | (g << 8) | (r << 0);
}

void DecodeMaterialPayload(
	in MaterialPayload payload,
	inout MaterialParam param)
{
	param.emissive = payload.emissive;
	param.hitT = payload.hitT;

	uint3 n = uint3(
		payload.normalRoughnessMetallic.x >> 16,
		payload.normalRoughnessMetallic.x & 0xffff,
		payload.normalRoughnessMetallic.y >> 16);
	param.normal = (float3(n) - 32767.0) * (1.0 / 32767.0);

	param.roughness = saturate(float((payload.normalRoughnessMetallic.y >> 8) & 0xff) / 255.0);
	param.metallic = saturate(float(payload.normalRoughnessMetallic.y & 0xff) / 255.0);

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
