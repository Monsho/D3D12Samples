struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD0;
};

VSOutput main(uint vid : SV_VertexID)
{
	VSOutput Out;

	float x = (float)(vid & 0x1);
	float y = (float)(vid >> 1);
	Out.position = float4(x * 2.0 - 1.0, 1.0 - y * 2.0, 0.0, 1.0);
	Out.uv = float2(x, y);

	return Out;
}

//	EOF
