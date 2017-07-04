#pragma once

#include <d3d12.h>

#include "sl12/pipeline_state.h"


namespace sl12
{
	//-----------------------------
	// RTブレンド記述子
	//-----------------------------
	RenderTargetBlendDesc DefaultRenderTargetBlendNone();
	RenderTargetBlendDesc DefaultRenderTargetBlendAlpha();
	RenderTargetBlendDesc DefaultRenderTargetBlendAdd();
	RenderTargetBlendDesc DefaultRenderTargetBlendSub();
	RenderTargetBlendDesc DefaultRenderTargetBlendMul();

	//-----------------------------
	// 深度ステート
	//-----------------------------
	DepthStencilDesc DefaultDepthStateDisableDisable();
	DepthStencilDesc DefaultDepthStateEnableDisable();
	DepthStencilDesc DefaultDepthStateEnableEnable();

	//-----------------------------
	// ラスタライザステート
	//-----------------------------
	RasterizerDesc DefaultRasterizerStateStandard(int depth_bias = D3D12_DEFAULT_DEPTH_BIAS, float depth_clamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP, float slope_bias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS);

}	// namespace sl12

//	EOF
