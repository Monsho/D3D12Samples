struct PSInput
{
	float4	position	: SV_POSITION;
};

float4 main(PSInput In) : SV_TARGET0
{
	return float4(1, 1, 1, 0.5);
}
