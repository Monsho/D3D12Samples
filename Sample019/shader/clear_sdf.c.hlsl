RWTexture3D<float>		rwSdf		: register(u0);

[numthreads(1, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	// 適当に大きな値でクリア
	// SDFはバウンディングボックスの大きさで正規化されるので、1.0以上ならOK
	rwSdf[dispatchID] = 10000.0;
}