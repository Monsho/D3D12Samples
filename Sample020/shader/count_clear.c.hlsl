RWByteAddressBuffer			outputCounter		: register(u0);

[numthreads(1, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint index = dispatchID.x;

	// 0ƒNƒŠƒA
	outputCounter.Store(index * 16, 0);
}