#pragma once

#include <d3d12.h>


namespace sl12
{
	//-----------------------------
	// RTブレンド記述子
	//-----------------------------
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendNone();
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendAlpha();
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendAdd();
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendSub();
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendMul();

	//-----------------------------
	// 深度ステート
	//-----------------------------
	D3D12_DEPTH_STENCIL_DESC DefaultDepthStateDisableDisable();
	D3D12_DEPTH_STENCIL_DESC DefaultDepthStateEnableDisable();
	D3D12_DEPTH_STENCIL_DESC DefaultDepthStateEnableEnable();

	//-----------------------------
	// ラスタライザステート
	//-----------------------------
	D3D12_RASTERIZER_DESC DefaultRasterizerStateStandard(int depth_bias = D3D12_DEFAULT_DEPTH_BIAS, float depth_clamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP, float slope_bias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS);

}	// namespace sl12

//	EOF
