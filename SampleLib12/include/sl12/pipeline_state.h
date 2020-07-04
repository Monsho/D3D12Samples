#pragma once

#include <vector>
#include <sl12/util.h>
#include <sl12/root_signature.h>


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
		Shader*					pAS = nullptr;
		Shader*					pMS = nullptr;
		BlendDesc				blend{};
		RasterizerDesc			rasterizer{};
		DepthStencilDesc		depthStencil{};
		InputLayoutDesc			inputLayout{};
		D3D_PRIMITIVE_TOPOLOGY	primTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		u32						numRTVs = 0;
		DXGI_FORMAT				rtvFormats[8]{ DXGI_FORMAT_UNKNOWN };
		DXGI_FORMAT				dsvFormat = DXGI_FORMAT_UNKNOWN;
		u32						multisampleCount = 1;
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
	};	// class ComputePipelineState


	class DxrPipelineStateDesc
	{
	public:
		DxrPipelineStateDesc()
		{}
		~DxrPipelineStateDesc()
		{
			for (auto&& v : descBinaries_)
			{
				free(v);
			}
			descBinaries_.clear();
		}

		void AddSubobject(D3D12_STATE_SUBOBJECT_TYPE type, const void* desc)
		{
			if (type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
			{
				exportAssociationIndices_.push_back((int)subobjects_.size());
			}

			D3D12_STATE_SUBOBJECT sub;
			sub.Type = type;
			sub.pDesc = desc;
			subobjects_.push_back(sub);
		}

		void AddDxilLibrary(const void* shaderBin, size_t shaderBinSize, D3D12_EXPORT_DESC* exportDescs, UINT exportDescsCount)
		{
			auto dxilDesc = AllocBinary< D3D12_DXIL_LIBRARY_DESC>();

			dxilDesc->DXILLibrary.pShaderBytecode = shaderBin;
			dxilDesc->DXILLibrary.BytecodeLength = shaderBinSize;
			dxilDesc->NumExports = exportDescsCount;
			dxilDesc->pExports = exportDescs;
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, dxilDesc);
		}

		void AddHitGroup(LPCWSTR hitGroupName, bool isTriangle, LPCWSTR anyHitName, LPCWSTR closestHitName, LPCWSTR intersectionName)
		{
			auto hitGroupDesc = AllocBinary<D3D12_HIT_GROUP_DESC>();

			hitGroupDesc->HitGroupExport = hitGroupName;
			hitGroupDesc->Type = isTriangle ? D3D12_HIT_GROUP_TYPE_TRIANGLES : D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
			hitGroupDesc->AnyHitShaderImport = anyHitName;
			hitGroupDesc->ClosestHitShaderImport = closestHitName;
			hitGroupDesc->IntersectionShaderImport = intersectionName;
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, hitGroupDesc);
		}

		void AddShaderConfig(UINT payloadSizeMax, UINT attributeSizeMax)
		{
			auto shaderConfigDesc = AllocBinary<D3D12_RAYTRACING_SHADER_CONFIG>();

			shaderConfigDesc->MaxPayloadSizeInBytes = payloadSizeMax;
			shaderConfigDesc->MaxAttributeSizeInBytes = attributeSizeMax;
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, shaderConfigDesc);
		}

		void AddLocalRootSignatureAndExportAssociation(sl12::RootSignature& localRootSig, LPCWSTR* exportsArray, UINT exportsCount)
		{
			auto localRS = AllocBinary<ID3D12RootSignature*>();

			*localRS = localRootSig.GetRootSignature();
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, localRS);

			if ((exportsArray != nullptr) && (exportsCount > 0))
			{
				auto assDesc = AllocBinary< D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION>();
				assDesc->pSubobjectToAssociate = nullptr;
				assDesc->NumExports = exportsCount;
				assDesc->pExports = exportsArray;
				AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, assDesc);
			}
		}

		void AddGlobalRootSignature(sl12::RootSignature& localRootSig)
		{
			auto globalRS = AllocBinary<ID3D12RootSignature*>();

			*globalRS = localRootSig.GetRootSignature();
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, globalRS);
		}

		void AddRaytracinConfig(UINT traceRayCount)
		{
			auto rtConfigDesc = AllocBinary<D3D12_RAYTRACING_PIPELINE_CONFIG>();

			rtConfigDesc->MaxTraceRecursionDepth = traceRayCount;
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, rtConfigDesc);
		}

		void AddStateObjectConfig(D3D12_STATE_OBJECT_FLAGS flags)
		{
			auto config = AllocBinary< D3D12_STATE_OBJECT_CONFIG>();

			config->Flags = flags;
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG, config);
		}

		void AddExistingCollection(ID3D12StateObject* pso, D3D12_EXPORT_DESC* exportDescs, UINT exportDescsCount)
		{
			auto desc = AllocBinary< D3D12_EXISTING_COLLECTION_DESC >();

			desc->pExistingCollection = pso;
			desc->NumExports = exportDescsCount;
			desc->pExports = exportDescs;
			AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION, desc);
		}

		D3D12_STATE_OBJECT_DESC GetStateObjectDesc(D3D12_STATE_OBJECT_TYPE type)
		{
			ResolveExportAssosiation();

			D3D12_STATE_OBJECT_DESC psoDesc{};
			psoDesc.Type = type;
			psoDesc.pSubobjects = subobjects_.data();
			psoDesc.NumSubobjects = (UINT)subobjects_.size();
			return psoDesc;
		}

	private:
		template <typename T>
		T* AllocBinary()
		{
			auto p = malloc(sizeof(T));
			descBinaries_.push_back(p);
			return reinterpret_cast<T*>(p);
		}

		void ResolveExportAssosiation()
		{
			auto subDesc = subobjects_.data();
			for (auto&& v : exportAssociationIndices_)
			{
				auto e = v;
				auto l = e - 1;

				auto eDesc = (D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*)subobjects_[e].pDesc;
				eDesc->pSubobjectToAssociate = subDesc + l;
			}
		}

	private:
		std::vector<D3D12_STATE_SUBOBJECT>	subobjects_;
		std::vector<int>					exportAssociationIndices_;
		std::vector<void*>					descBinaries_;
	};	// class DxrPipelineStateDesc

	class DxrPipelineState
	{
	public:
		DxrPipelineState()
		{}
		~DxrPipelineState()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, DxrPipelineStateDesc& dxrDesc, D3D12_STATE_OBJECT_TYPE type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);
		void Destroy();

		ID3D12StateObject* GetPSO() { return pDxrStateObject_; }

	private:
		ID3D12StateObject*			pDxrStateObject_ = nullptr;
	};	// class DxrPipelineState

}	// namespace sl12

//	EOF
