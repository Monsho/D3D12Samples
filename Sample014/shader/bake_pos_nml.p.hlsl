struct PSInput
{
	float4	position	: SV_POSITION;
	float3	mesh_pos	: POSITION;
	float3	mesh_nml	: NORMAL;
};

struct PSOutput
{
	float4	position	: SV_TARGET0;
	float4	normal		: SV_TARGET1;
};

PSOutput main(PSInput In)
{
	PSOutput Out;

	Out.position = float4(In.mesh_pos, 1);
	Out.normal = float4(In.mesh_nml, 1);

	return Out;
}
