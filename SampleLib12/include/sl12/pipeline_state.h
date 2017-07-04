#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class RootSignature;
	class Shader;
	class RenderTargetView;
	class DepthStencilView;

	struct RenderTargetBlendDesc
	{
		bool			isBlendEnable;
		bool			isLogicBlendEnable;
		D3D12_BLEND		srcBlendColor;
		D3D12_BLEND		dstBlendColor;
		D3D12_BLEND_OP	blendOpColor;
		D3D12_BLEND		srcBlendAlpha;
		D3D12_BLEND		dstBlendAlpha;
		D3D12_BLEND_OP	blendOpAlpha;
		D3D12_LOGIC_OP	logicOp;
		UINT8			writeMask;
	};	// struct RenderTargetBlendDesc

	struct BlendDesc
	{
		bool					isAlphaToCoverageEnable;
		bool					isIndependentBlend;
		u32						sampleMask;
		RenderTargetBlendDesc	rtDesc[8];
	};	// struct BlendDesc

	struct RasterizerDesc
	{
		D3D12_FILL_MODE		fillMode;
		D3D12_CULL_MODE		cullMode;
		bool				isFrontCCW;
		s32					depthBias;
		float				depthBiasClamp;
		float				slopeScaledDepthBias;
		bool				isDepthClipEnable;
		bool				isMultisampleEnable;
		bool				isAntialiasedLineEnable;
		bool				isConservativeRasterEnable;
	};	// struct RasterizerDesc

	struct DepthStencilDesc
	{
		bool						isDepthEnable;
		bool						isDepthWriteEnable;
		D3D12_COMPARISON_FUNC		depthFunc;
		bool						isStencilEnable;
		u8							stencilReadMask;
		u8							stencilWriteMask;
		D3D12_DEPTH_STENCILOP_DESC	stencilFrontFace;
		D3D12_DEPTH_STENCILOP_DESC	stencilBackFace;
	};	// struct DepthStencilDesc

	struct InputLayoutDesc
	{
		const D3D12_INPUT_ELEMENT_DESC*	pElements;
		u32								numElements;
	};	// struct InputLayoutDesc

	struct GraphicsPipelineStateDesc
	{
		RootSignature*			pRootSignature = nullptr;
		Shader*					pVS = nullptr;
		Shader*					pPS = nullptr;
		Shader*					pGS = nullptr;
		Shader*					pDS = nullptr;
		Shader*					pHS = nullptr;
		BlendDesc				blend{};
		RasterizerDesc			rasterizer{};
		DepthStencilDesc		depthStencil{};
		InputLayoutDesc			inputLayout{};
		D3D_PRIMITIVE_TOPOLOGY	primTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		u32						numRTVs = 0;
		RenderTargetView*		pRTVs[8]{};
		DepthStencilView*		pDSV = nullptr;
		u32						multisampleCount;
	};	// struct GraphicsPipelineStateDesc

	struct ComputePipelineStateDesc
	{
		RootSignature*			pRootSignature = nullptr;
		Shader*					pCS = nullptr;
	};	// struct ComputePipelineStateDesc

	class GraphicsPipelineState
	{
	public:
		GraphicsPipelineState()
		{}
		~GraphicsPipelineState()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const GraphicsPipelineStateDesc& desc);
		void Destroy();

		// getter
		ID3D12PipelineState* GetPSO() { return pPipelineState_; }

	private:
		ID3D12PipelineState*		pPipelineState_{ nullptr };
	};	// class GraphicsPipelineState

	class ComputePipelineState
	{
	public:
		ComputePipelineState()
		{}
		~ComputePipelineState()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const ComputePipelineStateDesc& desc);
		void Destroy();

		// getter
		ID3D12PipelineState* GetPSO() { return pPipelineState_; }

	private:
		ID3D12PipelineState*		pPipelineState_{ nullptr };
	};	// class GraphicsPipelineState

}	// namespace sl12

//	EOF
