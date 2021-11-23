struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

Texture2D		texImage	: register(t0);
SamplerState	samLinear	: register(s0);

float4 main(VSOutput In) : SV_Target0
{
	return texImage.SampleLevel(samLinear, In.uv, 0.0);
}

//	EOF
