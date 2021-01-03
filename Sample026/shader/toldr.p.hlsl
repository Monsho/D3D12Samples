struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

Texture2D	texHDR		: register(t0);

float4 main(VSOutput In) : SV_Target0
{
	float4 color = texHDR[In.position.xy];

	color.rgb = pow(saturate(color.rgb), 1 / 2.2);

	return color;
}

//	EOF
