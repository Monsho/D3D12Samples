#include <sl12/default_states.h>


namespace sl12
{
	//-----------------------------
	// ブレンドなしのRTブレンド記述子
	//-----------------------------
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendNone()
	{
		D3D12_RENDER_TARGET_BLEND_DESC ret{};
		ret.BlendEnable = false;
		ret.LogicOpEnable = false;
		ret.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return ret;
	}

	//-----------------------------
	// アルファブレンドのRTブレンド記述子
	//-----------------------------
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendAlpha()
	{
		D3D12_RENDER_TARGET_BLEND_DESC ret{};
		ret.BlendEnable = true;
		ret.LogicOpEnable = false;
		ret.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		ret.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		ret.BlendOp = D3D12_BLEND_OP_ADD;
		ret.SrcBlendAlpha = D3D12_BLEND_ONE;
		ret.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		ret.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		ret.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return ret;
	}

	//-----------------------------
	// アルファ加算ブレンドのRTブレンド記述子
	//-----------------------------
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendAdd()
	{
		D3D12_RENDER_TARGET_BLEND_DESC ret{};
		ret.BlendEnable = true;
		ret.LogicOpEnable = false;
		ret.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		ret.DestBlend = D3D12_BLEND_ONE;
		ret.BlendOp = D3D12_BLEND_OP_ADD;
		ret.SrcBlendAlpha = D3D12_BLEND_ONE;
		ret.DestBlendAlpha = D3D12_BLEND_ONE;
		ret.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		ret.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return ret;
	}

	//-----------------------------
	// アルファ減算ブレンドのRTブレンド記述子
	//-----------------------------
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendSub()
	{
		D3D12_RENDER_TARGET_BLEND_DESC ret{};
		ret.BlendEnable = true;
		ret.LogicOpEnable = false;
		ret.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		ret.DestBlend = D3D12_BLEND_ONE;
		ret.BlendOp = D3D12_BLEND_OP_SUBTRACT;
		ret.SrcBlendAlpha = D3D12_BLEND_ONE;
		ret.DestBlendAlpha = D3D12_BLEND_ONE;
		ret.BlendOpAlpha = D3D12_BLEND_OP_SUBTRACT;
		ret.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return ret;
	}

	//-----------------------------
	// 乗算ブレンドのRTブレンド記述子
	//-----------------------------
	D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendMul()
	{
		D3D12_RENDER_TARGET_BLEND_DESC ret{};
		ret.BlendEnable = true;
		ret.LogicOpEnable = false;
		ret.SrcBlend = D3D12_BLEND_DEST_COLOR;
		ret.DestBlend = D3D12_BLEND_ZERO;
		ret.BlendOp = D3D12_BLEND_OP_ADD;
		ret.SrcBlendAlpha = D3D12_BLEND_DEST_ALPHA;
		ret.DestBlendAlpha = D3D12_BLEND_ZERO;
		ret.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		ret.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return ret;
	}


	//-----------------------------
	// 深度比較なし・書き込みなし
	//-----------------------------
	D3D12_DEPTH_STENCIL_DESC DefaultDepthStateDisableDisable()
	{
		D3D12_DEPTH_STENCIL_DESC ret{};
		ret.DepthEnable = false;
		ret.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		ret.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		return ret;
	}

	//-----------------------------
	// 深度比較あり・書き込みなし
	//-----------------------------
	D3D12_DEPTH_STENCIL_DESC DefaultDepthStateEnableDisable()
	{
		D3D12_DEPTH_STENCIL_DESC ret{};
		ret.DepthEnable = true;
		ret.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		ret.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		return ret;
	}

	//-----------------------------
	// 深度比較あり・書き込みあり
	//-----------------------------
	D3D12_DEPTH_STENCIL_DESC DefaultDepthStateEnableEnable()
	{
		D3D12_DEPTH_STENCIL_DESC ret{};
		ret.DepthEnable = true;
		ret.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		ret.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		return ret;
	}


	//-----------------------------
	// 標準のラスタライザ
	//-----------------------------
	D3D12_RASTERIZER_DESC DefaultRasterizerStateStandard(int depth_bias, float depth_clamp, float slope_bias)
	{
		D3D12_RASTERIZER_DESC ret{};
		ret.FillMode = D3D12_FILL_MODE_SOLID;
		ret.CullMode = D3D12_CULL_MODE_FRONT;
		ret.FrontCounterClockwise = false;
		ret.DepthBias = depth_bias;
		ret.DepthBiasClamp = depth_clamp;
		ret.SlopeScaledDepthBias = slope_bias;
		ret.DepthClipEnable = true;
		ret.MultisampleEnable = false;
		ret.AntialiasedLineEnable = false;
		ret.ForcedSampleCount = 0;
		ret.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		return ret;
	}
}	// namespace sl12

	
	//	EOF
