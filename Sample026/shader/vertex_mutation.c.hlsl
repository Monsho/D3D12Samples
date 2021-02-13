#include "constant.h"

ConstantBuffer<VertexMutationCB>	cbVM		: register(b0);

StructuredBuffer<float3>	vertexPos			: register(t0);
StructuredBuffer<float3>	vertexNormal		: register(t1);

RWStructuredBuffer<float3>	outputPos			: register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint index = dispatchID.x;
	if (index < cbVM.vertexCount)
	{
		float3 pos = vertexPos[index];
		float3 n = pos * float3(1, 0, 1);
		float3 normal = length(n) < 1e-4 ? 0.0 : normalize(n);
		//float3 normal = vertexNormal[index];
		float t = pos.y + cbVM.time;
		float a = 2.0 * PI * t;
		float intensity = sin(a) * cbVM.mutateIntensity;
		outputPos[index] = pos + normal * intensity;
	}
}
