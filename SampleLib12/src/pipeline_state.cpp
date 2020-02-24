#include <sl12/pipeline_state.h>

#include <sl12/device.h>
#include <sl12/root_signature.h>
#include <sl12/shader.h>
#include <sl12/texture_view.h>


namespace sl12
{
	//----
	bool GraphicsPipelineState::Initialize(Device* pDev, const GraphicsPipelineStateDesc& desc)
	{
		if (!desc.pRootSignature)
		{
			return false;
		}

		auto rtBlendFunc = [&](D3D12_RENDER_TARGET_BLEND_DESC& dst, u32 index)
		{
			auto&& src = desc.blend.rtDesc[index];
			dst.BlendEnable = src.isBlendEnable;
			dst.LogicOpEnable = src.isLogicBlendEnable;
			dst.SrcBlend = src.srcBlendColor;
			dst.DestBlend = src.dstBlendColor;
			dst.BlendOp = src.blendOpColor;
			dst.SrcBlendAlpha = src.srcBlendAlpha;
			dst.DestBlendAlpha = src.dstBlendAlpha;
			dst.BlendOpAlpha = src.blendOpAlpha;
			dst.LogicOp = src.logicOp;
			dst.RenderTargetWriteMask = src.writeMask;
		};

		auto blendFunc = [&](D3D12_BLEND_DESC& dst)
		{
			dst.AlphaToCoverageEnable = desc.blend.isAlphaToCoverageEnable;
			dst.IndependentBlendEnable = desc.blend.isIndependentBlend;
			rtBlendFunc(dst.RenderTarget[0], 0);
			if (dst.IndependentBlendEnable)
			{
				for (u32 i = 1; i < 8; i++)
				{
					rtBlendFunc(dst.RenderTarget[i], i);
				}
			}
		};

		auto rasterFunc = [&](D3D12_RASTERIZER_DESC& dst)
		{
			dst.FillMode = desc.rasterizer.fillMode;
			dst.CullMode = desc.rasterizer.cullMode;
			dst.FrontCounterClockwise = desc.rasterizer.isFrontCCW;
			dst.DepthBias = desc.rasterizer.depthBias;
			dst.DepthBiasClamp = desc.rasterizer.depthBiasClamp;
			dst.SlopeScaledDepthBias = desc.rasterizer.slopeScaledDepthBias;
			dst.DepthClipEnable = desc.rasterizer.isDepthClipEnable;
			dst.MultisampleEnable = desc.rasterizer.isMultisampleEnable;
			dst.AntialiasedLineEnable = desc.rasterizer.isAntialiasedLineEnable;
			dst.ForcedSampleCount = 0;
			dst.ConservativeRaster = desc.rasterizer.isConservativeRasterEnable ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		};

		auto depthFunc = [&](D3D12_DEPTH_STENCIL_DESC& dst)
		{
			dst.DepthEnable = desc.depthStencil.isDepthEnable;
			dst.DepthWriteMask = desc.depthStencil.isDepthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
			dst.DepthFunc = desc.depthStencil.depthFunc;
			dst.StencilEnable = desc.depthStencil.isStencilEnable;
			dst.StencilReadMask = desc.depthStencil.stencilReadMask;
			dst.StencilWriteMask = desc.depthStencil.stencilWriteMask;
			dst.FrontFace = desc.depthStencil.stencilFrontFace;
			dst.BackFace = desc.depthStencil.stencilBackFace;
		};

		auto topoFunc = [&](D3D_PRIMITIVE_TOPOLOGY t)
		{
			D3D12_PRIMITIVE_TOPOLOGY_TYPE kTypes[] = {
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,		// D3D_PRIMITIVE_TOPOLOGY_UNDEFINED
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,			// D3D_PRIMITIVE_TOPOLOGY_POINTLIST
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,				// D3D_PRIMITIVE_TOPOLOGY_LINELIST
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,				// D3D_PRIMITIVE_TOPOLOGY_LINESTRIP
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,			// D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,			// D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,				// D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,				// D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,			// D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,			// D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ
			};
			return (t < D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST) ? kTypes[t] : D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { desc.inputLayout.pElements, desc.inputLayout.numElements };
		psoDesc.pRootSignature = desc.pRootSignature->GetRootSignature();
		if (desc.pVS)
		{
			psoDesc.VS = { reinterpret_cast<const UINT8*>(desc.pVS->GetData()), desc.pVS->GetSize() };
		}
		if (desc.pPS)
		{
			psoDesc.PS = { reinterpret_cast<const UINT8*>(desc.pPS->GetData()), desc.pPS->GetSize() };
		}
		if (desc.pGS)
		{
			psoDesc.GS = { reinterpret_cast<const UINT8*>(desc.pGS->GetData()), desc.pGS->GetSize() };
		}
		if (desc.pDS)
		{
			psoDesc.DS = { reinterpret_cast<const UINT8*>(desc.pDS->GetData()), desc.pDS->GetSize() };
		}
		if (desc.pHS)
		{
			psoDesc.HS = { reinterpret_cast<const UINT8*>(desc.pHS->GetData()), desc.pHS->GetSize() };
		}
		blendFunc(psoDesc.BlendState);
		psoDesc.SampleMask = desc.blend.sampleMask;
		rasterFunc(psoDesc.RasterizerState);
		depthFunc(psoDesc.DepthStencilState);
		psoDesc.PrimitiveTopologyType = topoFunc(desc.primTopology);
		psoDesc.NumRenderTargets = desc.numRTVs;
		for (u32 i = 0; i < desc.numRTVs; i++)
		{
			psoDesc.RTVFormats[i] = desc.rtvFormats[i];
		}
		psoDesc.DSVFormat = desc.dsvFormat;
		psoDesc.SampleDesc.Count = desc.multisampleCount;

		auto hr = pDev->GetDeviceDep()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pPipelineState_));
		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	//----
	void GraphicsPipelineState::Destroy()
	{
		SafeRelease(pPipelineState_);
	}


	//----
	bool ComputePipelineState::Initialize(Device* pDev, const ComputePipelineStateDesc& desc)
	{
		if (!desc.pRootSignature)
		{
			return false;
		}
		if (!desc.pCS)
		{
			return false;
		}

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = desc.pRootSignature->GetRootSignature();
		psoDesc.CS = { reinterpret_cast<const UINT8*>(desc.pCS->GetData()), desc.pCS->GetSize() };

		auto hr = pDev->GetDeviceDep()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pPipelineState_));
		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	//----
	void ComputePipelineState::Destroy()
	{
		SafeRelease(pPipelineState_);
	}


	//----
	bool DxrPipelineState::Initialize(Device* pDev, DxrPipelineStateDesc& dxrDesc, D3D12_STATE_OBJECT_TYPE type)
	{
		D3D12_STATE_OBJECT_DESC psoDesc = dxrDesc.GetStateObjectDesc(type);

		auto hr = pDev->GetDxrDeviceDep()->CreateStateObject(&psoDesc, IID_PPV_ARGS(&pDxrStateObject_));
		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	//----
	void DxrPipelineState::Destroy()
	{
		SafeRelease(pDxrStateObject_);
	}

}	// namespace sl12

//	EOF
