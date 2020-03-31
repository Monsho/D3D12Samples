struct VSOutput
{
	float4	pos			: SV_POSITION;
	float2	uv			: TEXCOORD0;
};

VSOutput main(uint VertexID : SV_VERTEXID)
{
	float u = (float)(VertexID & 0x01);
	float v = (float)(VertexID >> 0x01);

	VSOutput Out = (VSOutput)0;

	Out.pos = float4(
		u * 4.0 - 1.0,
		v * -4.0 + 1.0,
		0, 1);
	Out.uv = float2(u * 2.0, v * 2.0);

	return Out;
}

//	EOF
