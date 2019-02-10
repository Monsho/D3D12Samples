struct PSInput
{
	float4	position	: SV_POSITION;
	float3	normal		: NORMAL;
};

float4 main(PSInput In) : SV_TARGET0
{
	float3 normal = normalize(In.normal);
	return float4(normal * 0.5 + 0.5, 1);
}
