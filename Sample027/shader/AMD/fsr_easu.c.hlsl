struct CbFSR
{
	uint4 Const0;
	uint4 Const1;
	uint4 Const2;
	uint4 Const3;
};

ConstantBuffer<CbFSR>	cbFsr : register(b0);

#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

Texture2D			InputTexture : register(t0);
SamplerState		samLinearClamp : register(s0);

RWTexture2D<float4>	OutputTexture : register(u0);

#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { AF4 res = InputTexture.GatherRed(samLinearClamp, p, int2(0, 0)); return res; }
AF4 FsrEasuGF(AF2 p) { AF4 res = InputTexture.GatherGreen(samLinearClamp, p, int2(0, 0)); return res; }
AF4 FsrEasuBF(AF2 p) { AF4 res = InputTexture.GatherBlue(samLinearClamp, p, int2(0, 0)); return res; }

#include "ffx_fsr1.h"

void CurrFilter(int2 pos)
{
	AF3 c;
	FsrEasuF(c, pos, cbFsr.Const0, cbFsr.Const1, cbFsr.Const2, cbFsr.Const3);
	OutputTexture[pos] = float4(c, 1);
}

[numthreads(64, 1, 1)]
void main(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID, uint3 Dtid : SV_DispatchThreadID)
{
	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
	AU2 gxy = ARmp8x8(LocalThreadId.x) + AU2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
	CurrFilter(gxy);
	gxy.x += 8u;
	CurrFilter(gxy);
	gxy.y += 8u;
	CurrFilter(gxy);
	gxy.x -= 8u;
	CurrFilter(gxy);
}
