#include "common.hlsli"

ConstantBuffer<SceneCB>			cbScene			: register(b0);
ConstantBuffer<ReflectionCB>	cbReflection	: register(b1);

Texture2D			texPreview		: register(t0);
Texture2D			texMotionRM		: register(t1);
Texture2D<float>	texCurrDepth	: register(t2);
Texture2D<float>	texPrevDepth	: register(t3);
RWTexture2D<float4>	rwCurrent		: register(u0);
SamplerState		texPreview_s	: register(s0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint2 dst_pos = dispatchID.xy;
	uint w, h;
	rwCurrent.GetDimensions(w, h);
	if (dst_pos.x >= w || dst_pos.y >= h)
		return;

	// get current world position.
	float d = texCurrDepth[dst_pos];
	float2 uv = ((float2)dst_pos + 0.5) / cbScene.screenInfo.zw;
	float2 cs = uv * float2(2, -2) + float2(-1, 1);
	float4 cwp = mul(cbScene.mtxProjToWorld, float4(cs, d, 1));
	cwp.xyz *= 1 / cwp.w;

	// get preview world position.
	float2 motion = texMotionRM[dst_pos].xy;
	uv = uv - motion;
	if (any(uv < 0) || any(uv > 1))
		return;
	cs = uv * float2(2, -2) + float2(-1, 1);
	d = texPrevDepth.SampleLevel(texPreview_s, uv, 0);
	float4 pwp = mul(cbScene.mtxPrevProjToWorld, float4(cs, d, 1));
	pwp.xyz *= 1 / pwp.w;

	// motion blend.
	const float kLengthTh = 10.0;
	float2 pix_motion = motion * cbScene.screenInfo.zw;
	float lensq = dot(pix_motion, pix_motion);
	float s = lensq / (kLengthTh * kLengthTh);
	float blend_mo = 1 - saturate(s * s);

	// distance blend.
	const float kDistanceTh = 3.0;
	float4 curr = rwCurrent[dst_pos];
	float4 prev = texPreview.SampleLevel(texPreview_s, uv, 0);
	float distsq = dot(curr - prev, curr - prev);
	float t = distsq / (kDistanceTh * kDistanceTh);
	float blend_wp = 1 - saturate(t * t * t);

	float blend = blend_mo * blend_wp;
	rwCurrent[dst_pos] = lerp(prev, curr, saturate(lerp(1, cbReflection.currentBlendMax, blend)));
}

//	EOF
