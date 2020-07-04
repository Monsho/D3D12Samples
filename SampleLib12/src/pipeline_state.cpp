#include <sl12/pipeline_state.h>

#include <sl12/device.h>
#include <sl12/root_signature.h>
#include <sl12/shader.h>
#include <sl12/texture_view.h>


namespace sl12
{
	template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type, typename Data>
	struct alignas(void*) PipelineSubobject
	{
		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE	type;
		Data								data;

		PipelineSubobject()
			: type(Type)
			, data()
		{}
		PipelineSubobject(const Data& d)
			: PipelineSubobject()
			, data(d)
		{}
		PipelineSubobject operator=(const Data& d)
		{
			data = d;
			return *this;
		}
	};	// struct PipelineSubobject

	inline D3D12_SHADER_BYTECODE ToD3D12Shader(Shader* p)
	{
		D3D12_SHADER_BYTECODE ret = { reinterpret_cast<const UINT8*>(p->GetData()), p->GetSize() };
		return ret;
	}

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

		struct GraphicsPipelineDesc
		{
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature*>					pRootSignature;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE>							VS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, D3D12_SHADER_BYTECODE>							AS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, D3D12_SHADER_BYTECODE>							MS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE>							PS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS, D3D12_SHADER_BYTECODE>							GS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS, D3D12_SHADER_BYTECODE>							DS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS, D3D12_SHADER_BYTECODE>							HS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, D3D12_BLEND_DESC>								BlendState;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, UINT>									SampleMask;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC>					RasterizerState;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, D3D12_DEPTH_STENCIL_DESC>				DepthStencilState;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT, D3D12_INPUT_LAYOUT_DESC>				InputLayout;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE>	PrimitiveTopologyType;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY>			RenderTargets;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT>					DepthTarget;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DXGI_SAMPLE_DESC>						SampleDesc;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK, UINT>										NodeMask;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS, D3D12_PIPELINE_STATE_FLAGS>					Flags;
		};

		GraphicsPipelineDesc psoDesc = {};
		D3D12_INPUT_LAYOUT_DESC input_layout_desc = { desc.inputLayout.pElements, desc.inputLayout.numElements };
		psoDesc.InputLayout = input_layout_desc;
		psoDesc.pRootSignature = desc.pRootSignature->GetRootSignature();
		if (desc.pVS)
		{
			psoDesc.VS = ToD3D12Shader(desc.pVS);
		}
		else if (desc.pMS)
		{
			psoDesc.InputLayout.data.NumElements = 0;
			psoDesc.InputLayout.data.pInputElementDescs = nullptr;

			psoDesc.MS = ToD3D12Shader(desc.pMS);
			if (desc.pAS)
			{
				psoDesc.AS = ToD3D12Shader(desc.pAS);
			}
		}
		if (desc.pPS)
		{
			psoDesc.PS = ToD3D12Shader(desc.pPS);
		}
		if (desc.pGS)
		{
			psoDesc.GS = ToD3D12Shader(desc.pGS);
		}
		if (desc.pDS)
		{
			psoDesc.DS = ToD3D12Shader(desc.pDS);
		}
		if (desc.pHS)
		{
			psoDesc.HS = ToD3D12Shader(desc.pHS);
		}
		blendFunc(psoDesc.BlendState.data);
		psoDesc.SampleMask = desc.blend.sampleMask;
		rasterFunc(psoDesc.RasterizerState.data);
		depthFunc(psoDesc.DepthStencilState.data);
		psoDesc.PrimitiveTopologyType = topoFunc(desc.primTopology);

		D3D12_RT_FORMAT_ARRAY rt_formats = {};
		rt_formats.NumRenderTargets = desc.numRTVs;
		for (u32 i = 0; i < desc.numRTVs; i++)
		{
			rt_formats.RTFormats[i] = desc.rtvFormats[i];
		}
		psoDesc.RenderTargets = rt_formats;

		psoDesc.DepthTarget = desc.dsvFormat;

		DXGI_SAMPLE_DESC sample_desc = {};
		sample_desc.Count = desc.multisampleCount;
		psoDesc.SampleDesc = sample_desc;

		D3D12_PIPELINE_STATE_STREAM_DESC strmDesc = {};
		strmDesc.pPipelineStateSubobjectStream = &psoDesc;
		strmDesc.SizeInBytes = sizeof(psoDesc);
		auto hr = pDev->GetLatestDeviceDep()->CreatePipelineState(&strmDesc, IID_PPV_ARGS(&pPipelineState_));
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

		struct ComputePipelineDesc
		{
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, D3D12_SHADER_BYTECODE>			CS;
			PipelineSubobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature*>	pRootSignature;
		};
		ComputePipelineDesc d{};
		d.pRootSignature = desc.pRootSignature->GetRootSignature();
		d.CS = ToD3D12Shader(desc.pCS);

		D3D12_PIPELINE_STATE_STREAM_DESC strmDesc = {};
		strmDesc.pPipelineStateSubobjectStream = &d;
		strmDesc.SizeInBytes = sizeof(d);
		auto hr = pDev->GetLatestDeviceDep()->CreatePipelineState(&strmDesc, IID_PPV_ARGS(&pPipelineState_));

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
