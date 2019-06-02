struct PSInput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD0;
};

Texture2D		texLightMap	: register(t0);
Texture2D		texPosition	: register(t1);
Texture2D		texNormal	: register(t2);

float4 main(PSInput In) : SV_TARGET0
{
	uint2 uv = (uint2)In.position.xy;
	float4 base_p = texPosition[uv];
	float3 base_n = texNormal[uv].xyz;
	float3 base_c = texLightMap[uv].xyz * base_p.a;
	float weight = base_p.a;

	if (weight > 0)
	{
		const float kMaxLength = 20.0;
		const int kBlockSize = 5;
		for (int x = -kBlockSize; x <= kBlockSize; x++)
		{
			for (int y = -kBlockSize; y <= kBlockSize; y++)
			{
				if (x == 0 && y == 0)
					continue;

				float4 p = texPosition[uv + uint2(x, y)];
				float3 n = texNormal[uv + uint2(x, y)].xyz;
				float3 c = texLightMap[uv + uint2(x, y)].xyz;

				float w = p.w;
				w *= smoothstep(kMaxLength, 0.0, length(base_p.xyz - p.xyz));
				w *= smoothstep(0.0, 0.2, dot(base_n, n));
				base_c += c * w;
				weight += w;
			}
		}
	}
	else
	{
		for (int x = -1; x <= 1; x++)
		{
			for (int y = -1; y <= 1; y++)
			{
				float4 p = texPosition[uv + uint2(x, y)];
				float3 c = texLightMap[uv + uint2(x, y)].xyz;

				float w = p.w;
				base_c += c * w;
				weight += w;
			}
		}
	}
	base_c /= weight;

	return float4(base_c, 1);
}
