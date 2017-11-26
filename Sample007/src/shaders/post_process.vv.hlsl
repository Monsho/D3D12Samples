struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

VSOutput main(uint index : SV_VertexID)
{
	VSOutput Out;

	Out.position.x = (float)(index & 0x1) * 4 - 1;
	Out.position.y = (float)((index >> 1) & 0x1) * -4 + 1;
	Out.position.zw = float2(0, 1);

	Out.uv.x = (float)(index & 0x1) * 2;
	Out.uv.y = (float)((index >> 1) & 0x1) * 2;

	return Out;
}

//	EOF
