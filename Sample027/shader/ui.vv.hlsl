#include "constant.h"

struct VSOutput
{
	float4	position	: SV_POSITION;
	float2	uv			: TEXCOORD;
};

ConstantBuffer<SceneCB>		cbScene	: register(b0);
ConstantBuffer<UIDrawCB>	cbUI	: register(b1);

VSOutput main(uint index : SV_VertexID)
{
	VSOutput Out;

	float2 Center = cbScene.screenInfo.zw * 0.5;
	float2 LeftTop = (cbUI.rect.xy - Center) / Center;
	float2 WidthHeight = (cbUI.rect.zw - cbUI.rect.xy) / Center;

	Out.position.x = (index & 0x1) == 0x0 ? cbUI.rect.x : cbUI.rect.z;
	Out.position.y = ((index >> 1) & 0x1) == 0x0 ? cbUI.rect.y : cbUI.rect.w;
	Out.position.zw = float2(0, 1);

	Out.uv.x = (float)(index & 0x1);
	Out.uv.y = (float)((index >> 1) & 0x1);

	return Out;
}

//	EOF
