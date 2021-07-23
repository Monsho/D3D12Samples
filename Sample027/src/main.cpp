#include <vector>
#include <random>

#include "sl12/application.h"
#include "sl12/command_list.h"
#include "sl12/root_signature.h"
#include "sl12/texture.h"
#include "sl12/texture_view.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/command_queue.h"
#include "sl12/descriptor.h"
#include "sl12/descriptor_heap.h"
#include "sl12/swapchain.h"
#include "sl12/pipeline_state.h"
#include "sl12/acceleration_structure.h"
#include "sl12/file.h"
#include "sl12/shader.h"
#include "sl12/gui.h"
#include "sl12/glb_mesh.h"
#include "sl12/timestamp.h"
#include "sl12/descriptor_set.h"
#include "sl12/fence.h"
#include "sl12/death_list.h"
#include "sl12/resource_loader.h"
#include "sl12/resource_mesh.h"
#include "sl12/resource_texture.h"
#include "sl12/shader_manager.h"
#include "sl12/constant_buffer_cache.h"

#include "scene_mesh.h"

#define USE_IN_CPP
#include "../shader/constant.h"
#undef USE_IN_CPP

#include <windowsx.h>

#define LANE_COUNT_IN_WAVE	32

namespace
{
	static const int	kScreenWidth = 2560;
	static const int	kScreenHeight = 1440;
	static const int	MaxSample = 512;
	static const float	kNearZ = 0.01f;
	static const float	kFarZ = 100.0f;

	static const float	kSponzaScale = 20.0f;
	static const float	kSuzanneScale = 1.0f;

	static const std::string	kShaderDir("../Sample027/shader/");

	static const char* kShaderFiles[] =
	{
		"prepass.vv.hlsl",
		"prepass.p.hlsl",
		"gbuffer.vv.hlsl",
		"gbuffer.p.hlsl",
		"lighting.c.hlsl",
		"toldr.p.hlsl",
		"reduce_depth_1st.p.hlsl",
		"reduce_depth_2nd.p.hlsl",
		"fullscreen.vv.hlsl",
		"occlusion.lib.hlsl",
		"material.lib.hlsl",
		"direct_shadow.lib.hlsl",
		"cluster_cull.c.hlsl",
		"reset_cull_data.c.hlsl",
		"cull_1st_phase.c.hlsl",
		"cull_2nd_phase.c.hlsl",
	};

	enum ShaderFileKind
	{
		SHADER_PREPASS_VV,
		SHADER_PREPASS_P,
		SHADER_GBUFFER_VV,
		SHADER_GBUFFER_P,
		SHADER_LIGHTING_C,
		SHADER_TOLDR_P,
		SHADER_REDUCE_DEPTH_1ST_P,
		SHADER_REDUCE_DEPTH_2ND_P,
		SHADER_FULLSCREEN_VV,
		SHADER_OCCLUSION_LIB,
		SHADER_MATERIAL_LIB,
		SHADER_DIRECT_SHADOW_LIB,
		SHADER_CLUSTER_CULL_C,
		SHADER_RESET_CULL_DATA_C,
		SHADER_CULL_1ST_PHASE_C,
		SHADER_CULL_2ND_PHASE_C,

		SHADER_MAX
	};

	static const int kRTMaterialTableCount = 2;
	static LPCWSTR kOcclusionCHS = L"OcclusionCHS";
	static LPCWSTR kOcclusionAHS = L"OcclusionAHS";
	static LPCWSTR kOcclusionOpacityHG = L"OcclusionOpacityHG";
	static LPCWSTR kOcclusionMaskedHG = L"OcclusionMaskedHG";
	static LPCWSTR kMaterialCHS = L"MaterialCHS";
	static LPCWSTR kMaterialAHS = L"MaterialAHS";
	static LPCWSTR kMaterialOpacityHG = L"MaterialOpacityHG";
	static LPCWSTR kMaterialMaskedHG = L"MaterialMaskedHG";
	static LPCWSTR kDirectShadowRGS = L"DirectShadowRGS";
	static LPCWSTR kDirectShadowMS = L"DirectShadowMS";
	static LPCWSTR kGlobalIlluminationRGS = L"GlobalIlluminationRGS";
	static LPCWSTR kGlobalIlluminationMS = L"GlobalIlluminationMS";

	static const DXGI_FORMAT	kReytracingResultFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	class MeshletRenderComponent
	{
	public:
		MeshletRenderComponent()
		{}
		~MeshletRenderComponent()
		{
			Destroy();
		}

		bool Initialize(sl12::Device* pDev, const std::vector<sl12::ResourceItemMesh::Meshlet>& meshlets)
		{
			struct MeshletData
			{
				DirectX::XMFLOAT3	aabbMin;
				sl12::u32			indexOffset;
				DirectX::XMFLOAT3	aabbMax;
				sl12::u32			indexCount;
			};
			struct MeshShaderMeshlet
			{
				DirectX::XMFLOAT3	aabbMin;
				sl12::u32			primitiveOffset;
				DirectX::XMFLOAT3	aabbMax;
				sl12::u32			primitiveCount;
				DirectX::XMFLOAT3	coneApex;
				sl12::u32			vertexIndexOffset;
				DirectX::XMFLOAT3	coneAxis;
				sl12::u32			vertexIndexCount;
				float				coneCutoff;
			};

			if (!meshletB_.Initialize(pDev, sizeof(MeshletData) * meshlets.size(), sizeof(MeshletData), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}
			if (!meshletBV_.Initialize(pDev, &meshletB_, 0, (sl12::u32)meshlets.size(), sizeof(MeshletData)))
			{
				return false;
			}

			if (!meshletForMSB_.Initialize(pDev, sizeof(MeshShaderMeshlet) * meshlets.size(), sizeof(MeshShaderMeshlet), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}
			if (!meshletForMSBV_.Initialize(pDev, &meshletForMSB_, 0, (sl12::u32)meshlets.size(), sizeof(MeshShaderMeshlet)))
			{
				return false;
			}

			if (!indirectArgumentB_.Initialize(pDev, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshlets.size(), sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, false, true))
			{
				return false;
			}
			if (!indirectArgumentUAV_.Initialize(pDev, &indirectArgumentB_, 0, (sl12::u32)meshlets.size(), sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), 0))
			{
				return false;
			}

			{
				MeshletData* data = (MeshletData*)meshletB_.Map(nullptr);
				for (auto&& m : meshlets)
				{
					data->aabbMin = m.boundingInfo.box.aabbMin;
					data->aabbMax = m.boundingInfo.box.aabbMax;
					data->indexOffset = m.indexOffset;
					data->indexCount = m.indexCount;
					data++;
				}
				meshletB_.Unmap();
			}
			{
				MeshShaderMeshlet* data = (MeshShaderMeshlet*)meshletForMSB_.Map(nullptr);
				for (auto&& m : meshlets)
				{
					data->aabbMin = m.boundingInfo.box.aabbMin;
					data->aabbMax = m.boundingInfo.box.aabbMax;
					data->coneApex = m.boundingInfo.cone.apex;
					data->coneAxis = m.boundingInfo.cone.axis;
					data->coneCutoff = m.boundingInfo.cone.cutoff;
					data->primitiveOffset = m.primitiveOffset;
					data->primitiveCount = m.primitiveCount;
					data->vertexIndexOffset = m.vertexIndexOffset;
					data->vertexIndexCount = m.vertexIndexCount;
					data++;
				}
				meshletForMSB_.Unmap();
			}

			return true;
		}

		void Destroy()
		{
			meshletBV_.Destroy();
			meshletB_.Destroy();
			meshletForMSBV_.Destroy();
			meshletForMSB_.Destroy();
			indirectArgumentUAV_.Destroy();
			indirectArgumentB_.Destroy();
		}

		void TransitionIndirectArgument(sl12::CommandList* pCmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
		{
			pCmdList->TransitionBarrier(&indirectArgumentB_, before, after);
		}

		sl12::BufferView& GetMeshletBV()
		{
			return meshletBV_;
		}
		sl12::BufferView& GetMeshletForMSBV()
		{
			return meshletForMSBV_;
		}
		sl12::Buffer& GetIndirectArgumentB()
		{
			return indirectArgumentB_;
		}
		sl12::UnorderedAccessView& GetIndirectArgumentUAV()
		{
			return indirectArgumentUAV_;
		}

	private:
		sl12::Buffer				meshletB_;
		sl12::BufferView			meshletBV_;
		sl12::Buffer				meshletForMSB_;
		sl12::BufferView			meshletForMSBV_;
		sl12::Buffer				indirectArgumentB_;
		sl12::UnorderedAccessView	indirectArgumentUAV_;
	};	// class MeshletRenderComponent
}

class SampleApplication
	: public sl12::Application
{
	struct Sphere
	{
		DirectX::XMFLOAT3	center;
		float				radius;
		DirectX::XMFLOAT4	color;
		uint32_t			material;

		Sphere()
			: center(0.0f, 0.0f, 0.0f), radius(1.0f), color(1.0f, 1.0f, 1.0f, 1.0f), material(0)
		{}
		Sphere(const DirectX::XMFLOAT3& c, float r, const DirectX::XMFLOAT4& col, uint32_t mat)
			: center(c), radius(r), color(col), material(mat)
		{}
		Sphere(float cx, float cy, float cz, float r, const DirectX::XMFLOAT4& col, uint32_t mat)
			: center(cx, cy, cz), radius(r), color(col), material(mat)
		{}
	};

	struct Instance
	{
		DirectX::XMFLOAT4X4	mtxLocalToWorld;
		DirectX::XMFLOAT4	color;
		uint32_t			material;
	};

public:
	SampleApplication(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
		: Application(hInstance, nCmdShow, screenWidth, screenHeight)
	{}

	bool Initialize() override
	{
		// リソースロード開始
		if (!resLoader_.Initialize(&device_))
		{
			return false;
		}
		hBlueNoiseRes_ = resLoader_.LoadRequest<sl12::ResourceItemTexture>("data/blue_noise.tga");
		hMeshRes_ = resLoader_.LoadRequest<sl12::ResourceItemMesh>("data/sponza/sponza.rmesh");

		// initialize const buffer cache.
		cbvCache_.Initialize(&device_);

		// shader compile.
		if (!shaderManager_.Initialize(&device_, nullptr))
		{
			return false;
		}
		{
			for (int i = 0; i < SHADER_MAX; i++)
			{
				hShaders_[i] = shaderManager_.CompileFromFile(
					kShaderDir + kShaderFiles[i],
					"main",
					sl12::GetShaderTypeFromFileName(kShaderFiles[i]), 6, 5, nullptr, nullptr);
			}
			while (shaderManager_.IsCompiling())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		// コマンドリストの初期化
		auto&& gqueue = device_.GetGraphicsQueue();
		auto&& cqueue = device_.GetComputeQueue();
		if (!zpreCmdLists_.Initialize(&device_, &gqueue))
		{
			return false;
		}
		if (!litCmdLists_.Initialize(&device_, &gqueue))
		{
			return false;
		}
		if (!utilCmdList_.Initialize(&device_, &gqueue, true))
		{
			return false;
		}

		// Gバッファを生成
		for (int i = 0; i < ARRAYSIZE(gbuffers_); i++)
		{
			if (!gbuffers_[i].Initialize(&device_, kScreenWidth, kScreenHeight))
			{
				return false;
			}
		}

		{
			sl12::TextureDesc desc;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.format = DXGI_FORMAT_R11G11B10_FLOAT;
			desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			desc.isRenderTarget = true;
			desc.isUav = true;
			if (!accumTex_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!accumSRV_.Initialize(&device_, &accumTex_))
			{
				return false;
			}

			if (!accumRTV_.Initialize(&device_, &accumTex_))
			{
				return false;
			}

			if (!accumUAV_.Initialize(&device_, &accumTex_))
			{
				return false;
			}
		}

		// create HiZ.
		{
			sl12::TextureDesc desc;
			desc.width = kScreenWidth / 2;
			desc.height = kScreenHeight / 2;
			desc.format = DXGI_FORMAT_R16G16_FLOAT;
			desc.mipLevels = HIZ_MIP_LEVEL;
			desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.isRenderTarget = true;
			desc.isUav = false;
			if (!HiZTex_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!HiZSrv_.Initialize(&device_, &HiZTex_))
			{
				return false;
			}
			for (int i = 0; i < HIZ_MIP_LEVEL; i++)
			{
				if (!HiZSubSrvs_[i].Initialize(&device_, &HiZTex_, i, 1))
				{
					return false;
				}
				if (!HiZSubRtvs_[i].Initialize(&device_, &HiZTex_, i))
				{
					return false;
				}
			}
		}

		// create draw count buffer.
		{
			if (!DrawCountBuffer_.Initialize(&device_, sizeof(sl12::u32), 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false, true))
			{
				return false;
			}
			if (!DrawCountUAV_.Initialize(&device_, &DrawCountBuffer_, 0, 0, 0, 0))
			{
				return false;
			}
			for (int i = 0; i < kBufferCount; ++i)
			{
				if (!DrawCountReadbacks_[i].Initialize(&device_, sizeof(sl12::u32), 0, sl12::BufferUsage::ReadBack, D3D12_RESOURCE_STATE_COPY_DEST, false, false))
				{
					return false;
				}
			}
		}

		// サンプラー作成
		{
			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			if (!imageSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}
		{
			D3D12_SAMPLER_DESC desc{};
			desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.MaxLOD = FLT_MAX;
			desc.Filter = D3D12_FILTER_ANISOTROPIC;
			desc.MaxAnisotropy = 8;

			anisoSampler_.Initialize(&device_, desc);
		}

		// create root signature.
		if (!PrePassRS_.Initialize(&device_,
			hShaders_[SHADER_PREPASS_VV].GetShader(),
			hShaders_[SHADER_PREPASS_P].GetShader(),
			nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!GBufferRS_.Initialize(&device_,
			hShaders_[SHADER_GBUFFER_VV].GetShader(),
			hShaders_[SHADER_GBUFFER_P].GetShader(),
			nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!ToLdrRS_.Initialize(&device_,
			hShaders_[SHADER_FULLSCREEN_VV].GetShader(),
			hShaders_[SHADER_TOLDR_P].GetShader(),
			nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!ReduceDepth1stRS_.Initialize(&device_,
			hShaders_[SHADER_FULLSCREEN_VV].GetShader(),
			hShaders_[SHADER_REDUCE_DEPTH_1ST_P].GetShader(),
			nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!ReduceDepth2ndRS_.Initialize(&device_,
			hShaders_[SHADER_FULLSCREEN_VV].GetShader(),
			hShaders_[SHADER_REDUCE_DEPTH_2ND_P].GetShader(),
			nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!ClusterCullRS_.Initialize(&device_,
			hShaders_[SHADER_CLUSTER_CULL_C].GetShader()))
		{
			return false;
		}
		if (!ResetCullDataRS_.Initialize(&device_,
			hShaders_[SHADER_RESET_CULL_DATA_C].GetShader()))
		{
			return false;
		}
		if (!Cull1stPhaseRS_.Initialize(&device_,
			hShaders_[SHADER_CULL_1ST_PHASE_C].GetShader()))
		{
			return false;
		}
		if (!Cull2ndPhaseRS_.Initialize(&device_,
			hShaders_[SHADER_CULL_2ND_PHASE_C].GetShader()))
		{
			return false;
		}
		if (!LightingRS_.Initialize(&device_,
			hShaders_[SHADER_LIGHTING_C].GetShader()))
		{
			return false;
		}

		// create pipeline state.
		{
			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &PrePassRS_;
			desc.pVS = hShaders_[SHADER_PREPASS_VV].GetShader();
			desc.pPS = hShaders_[SHADER_PREPASS_P].GetShader();

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_BACK;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = true;

			desc.depthStencil.isDepthEnable = true;
			desc.depthStencil.isDepthWriteEnable = true;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			desc.inputLayout.numElements = ARRAYSIZE(input_elems);
			desc.inputLayout.pElements = input_elems;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.multisampleCount = 1;

			if (!PrePassPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &GBufferRS_;
			desc.pVS = hShaders_[SHADER_GBUFFER_VV].GetShader();
			desc.pPS = hShaders_[SHADER_GBUFFER_P].GetShader();

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_BACK;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = true;

			desc.depthStencil.isDepthEnable = true;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_EQUAL;

			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			desc.inputLayout.numElements = ARRAYSIZE(input_elems);
			desc.inputLayout.pElements = input_elems;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			for (int i = 0; i < ARRAYSIZE(gbuffers_[0].gbufferTex); i++)
			{
				desc.rtvFormats[desc.numRTVs++] = gbuffers_[0].gbufferTex[i].GetTextureDesc().format;
			}
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.multisampleCount = 1;

			if (!GBufferPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &ToLdrRS_;
			desc.pVS = hShaders_[SHADER_FULLSCREEN_VV].GetShader();
			desc.pPS = hShaders_[SHADER_TOLDR_P].GetShader();

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!ToLdrPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &ReduceDepth1stRS_;
			desc.pVS = hShaders_[SHADER_FULLSCREEN_VV].GetShader();
			desc.pPS = hShaders_[SHADER_REDUCE_DEPTH_1ST_P].GetShader();

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R16G16_FLOAT;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!ReduceDepth1stPSO_.Initialize(&device_, desc))
			{
				return false;
			}

			desc.pRootSignature = &ReduceDepth2ndRS_;
			desc.pPS = hShaders_[SHADER_REDUCE_DEPTH_2ND_P].GetShader();

			if (!ReduceDepth2ndPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &ClusterCullRS_;
			desc.pCS = hShaders_[SHADER_CLUSTER_CULL_C].GetShader();

			if (!ClusterCullPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &ResetCullDataRS_;
			desc.pCS = hShaders_[SHADER_RESET_CULL_DATA_C].GetShader();

			if (!ResetCullDataPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &Cull1stPhaseRS_;
			desc.pCS = hShaders_[SHADER_CULL_1ST_PHASE_C].GetShader();

			if (!Cull1stPhasePSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &Cull2ndPhaseRS_;
			desc.pCS = hShaders_[SHADER_CULL_2ND_PHASE_C].GetShader();

			if (!Cull2ndPhasePSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &LightingRS_;
			desc.pCS = hShaders_[SHADER_LIGHTING_C].GetShader();

			if (!LightingPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// ポイントライト
		if (!CreatePointLights(DirectX::XMFLOAT3(-20.0f, -10.0f, -15.0f), DirectX::XMFLOAT3(20.0f, 10.0f, 15.0f), 3.0f, 10.0f, 1.0f, 10.0f))
		{
			return false;
		}

		// タイムスタンプクエリ
		for (int i = 0; i < ARRAYSIZE(gpuTimestamp_); ++i)
		{
			if (!gpuTimestamp_[i].Initialize(&device_, 10))
			{
				return false;
			}
		}

		utilCmdList_.Reset();

		// GUIの初期化
		if (!gui_.Initialize(&device_, DXGI_FORMAT_R8G8B8A8_UNORM))
		{
			return false;
		}
		if (!gui_.CreateFontImage(&device_, utilCmdList_))
		{
			return false;
		}

		utilCmdList_.Close();
		utilCmdList_.Execute();
		device_.WaitDrawDone();

		sceneState_ = 0;

		return true;
	}

	bool Execute() override
	{
		device_.WaitPresent();
		device_.SyncKillObjects();

		if (sceneState_ == 0)
		{
			// loading scene
			ExecuteLoadingScene();
		}
		else if (sceneState_ == 1)
		{
			// main scene
			ExecuteMainScene();
		}

		return true;
	}

	void ExecuteLoadingScene()
	{
		const int kSwapchainBufferOffset = 1;
		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto prevFrameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 2) % sl12::Swapchain::kMaxBuffer;
		auto&& zpreCmdList = zpreCmdLists_.Reset();
		auto pCmdList = &zpreCmdList;
		auto d3dCmdList = pCmdList->GetCommandList();
		auto&& curGBuffer = gbuffers_[frameIndex];
		auto&& prevGBuffer = gbuffers_[prevFrameIndex];

		gui_.BeginNewFrame(pCmdList, kScreenWidth, kScreenHeight, inputData_);
		{
			static int sElapsedFrame = 0;
			std::string text = "now loading";
			for (int i = 0; i < sElapsedFrame; i++) text += '.';
			sElapsedFrame = (sElapsedFrame + 1) % 5;
			ImGui::Text(text.c_str());
		}

		device_.LoadRenderCommands(pCmdList);

		// clear swapchain.
		auto&& swapchain = device_.GetSwapchain();
		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle, color, 0, nullptr);
		}

		// set render target.
		{
			auto&& rtv = swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle;
			auto&& dsv = curGBuffer.depthDSV.GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(1, &rtv, false, &dsv);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = kScreenWidth;
			vp.Height = kScreenHeight;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = kScreenWidth;
			rect.bottom = kScreenHeight;
			d3dCmdList->RSSetScissorRects(1, &rect);
		}

		// draw imgui.
		ImGui::Render();

		// barrier swapchain.
		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		// コマンド終了と描画待ち
		zpreCmdLists_.Close();
		device_.WaitDrawDone();

		// 次のフレームへ
		device_.Present(0);

		// コマンド実行
		zpreCmdLists_.Execute();

		// complete resource load.
		if (!resLoader_.IsLoading())
		{
			CreateIndirectDrawParams();

			CreateSceneCB();
			CreateMeshCB();

			InitializeRaytracingPipeline();
#if 0
			utilCmdList_.Reset();
			CreateAS(&utilCmdList_);
			utilCmdList_.Close();
			utilCmdList_.Execute();
			device_.WaitDrawDone();
#else
			std::vector<int> TableOffsets;
			std::vector<sl12::Buffer*> InfoBuffers;

			utilCmdList_.Reset();
			CreateCompactionBLAS(&utilCmdList_, TableOffsets);
			utilCmdList_.Close();
			utilCmdList_.Execute();
			device_.WaitDrawDone();

			utilCmdList_.Reset();
			CompactBLAS(&utilCmdList_, InfoBuffers);
			CreateTLAS(&utilCmdList_, TableOffsets);
			utilCmdList_.Close();
			utilCmdList_.Execute();
			device_.WaitDrawDone();

			for (auto&& buff : InfoBuffers)
			{
				device_.KillObject(buff);
			}

			sponzaMesh_ = std::make_unique<SceneMesh>(&device_, hMeshRes_.GetItem<sl12::ResourceItemMesh>());

			DirectX::XMMATRIX m = DirectX::XMMatrixScaling(kSponzaScale, kSponzaScale, kSponzaScale);
			DirectX::XMFLOAT4X4 mf;
			DirectX::XMStoreFloat4x4(&mf, m);
			sponzaMesh_->SetMtxLocalToWorld(mf);
#endif
			InitializeRaytracingResource();

			sceneState_ = 1;
		}
	}

	void ExecuteMainScene()
	{
		const int kSwapchainBufferOffset = 1;
		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto prevFrameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 2) % sl12::Swapchain::kMaxBuffer;
		auto&& zpreCmdList = zpreCmdLists_.Reset();
		auto&& litCmdList = litCmdLists_.Reset();
		auto pCmdList = &zpreCmdList;
		auto d3dCmdList = pCmdList->GetLatestCommandList();
		auto&& curGBuffer = gbuffers_[frameIndex];
		auto&& prevGBuffer = gbuffers_[prevFrameIndex];

		UpdateSceneCB(frameIndex);

		gui_.BeginNewFrame(&litCmdList, kScreenWidth, kScreenHeight, inputData_);

		// カメラ操作
		ControlCamera();
		if (isCameraMove_)
		{
			giSampleTotal_ = 0;
			isCameraMove_ = false;
		}

		// GUI
		{
			if (ImGui::SliderFloat("Sky Power", &skyPower_, 0.0f, 10.0f))
			{
			}
			if (ImGui::SliderFloat("Light Intensity", &lightPower_, 0.0f, 10.0f))
			{
			}
			if (ImGui::ColorEdit3("Light Color", lightColor_))
			{
			}
			if (ImGui::SliderFloat2("Roughness Range", (float*)&roughnessRange_, 0.0f, 1.0f))
			{
			}
			if (ImGui::SliderFloat2("Metallic Range", (float*)&metallicRange_, 0.0f, 1.0f))
			{
			}
			if (ImGui::Checkbox("Occlusion Cull", &isOcclusionCulling_))
			{
				isOcclusionReset_ = true;
			}
			if (ImGui::Checkbox("Freeze Cull", &isFreezeCull_))
			{
				isOcclusionReset_ = true;
			}

			// draw count.
			auto draw_count = (sl12::u32*)DrawCountReadbacks_[frameIndex].Map(nullptr);
			ImGui::Text("Draw Count: %d", *draw_count);
			DrawCountReadbacks_[frameIndex].Unmap();

			// time stamp.
			uint64_t freq = device_.GetGraphicsQueue().GetTimestampFrequency();
			uint64_t timestamp[6];

			gpuTimestamp_[frameIndex].GetTimestamp(0, 6, timestamp);
			uint64_t all_time = timestamp[2] - timestamp[0];
			float all_ms = (float)all_time / ((float)freq / 1000.0f);

			ImGui::Text("All GPU: %f (ms)", all_ms);
		}

		gpuTimestamp_[frameIndex].Reset();
		gpuTimestamp_[frameIndex].Query(pCmdList);

		device_.LoadRenderCommands(pCmdList);

		auto&& swapchain = device_.GetSwapchain();
		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle, color, 0, nullptr);
		}

		d3dCmdList->ClearDepthStencilView(curGBuffer.depthDSV.GetDescInfo().cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[0], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[1], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[2], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

		gpuTimestamp_[frameIndex].Query(pCmdList);

		D3D12_SHADING_RATE_COMBINER shading_rate_combiners[] = {
			D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
			D3D12_SHADING_RATE_COMBINER_OVERRIDE,
		};

		// cluster culling.
		{
			pCmdList->TransitionBarrier(&clusterInfoBuffer_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			descSet_.Reset();
			descSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(0, pointLightsPosSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsUav(0, clusterInfoUAV_.GetDescInfo().cpuHandle);

			d3dCmdList->SetPipelineState(ClusterCullPSO_.GetPSO());
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&ClusterCullRS_, &descSet_);
			d3dCmdList->Dispatch(1, 1, CLUSTER_DIV_Z);
		}

		// 1st phase culling.
		if (!isFreezeCull_)
		{
			descSet_.Reset();
			descSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetCsCbv(1, frustumCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(2, HiZSrv_.GetDescInfo().cpuHandle);

			SceneMesh* pSceneMeshes[] = {
				sponzaMesh_.get(),
			};
			for (auto mesh : pSceneMeshes)
			{
				sl12::u32 submeshCount = mesh->GetSubmeshCount();
				pCmdList->TransitionBarrier(mesh->GetIndirectArgBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				pCmdList->TransitionBarrier(mesh->GetIndirectCountBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				// reset count buffer.
				descSet_.SetCsUav(0, mesh->GetIndirectCountUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(1, mesh->GetFalseNegativeCountUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(2, DrawCountUAV_.GetDescInfo().cpuHandle);
				d3dCmdList->SetPipelineState(ResetCullDataPSO_.GetPSO());
				pCmdList->SetComputeRootSignatureAndDescriptorSet(&ResetCullDataRS_, &descSet_);
				d3dCmdList->Dispatch((submeshCount + 31) / 32, 1, 1);

				// cull frustum, backface and prev depth occlusion.
				descSet_.SetCsCbv(2, glbMeshCBV_.GetDescInfo().cpuHandle);
				descSet_.SetCsUav(0, mesh->GetIndirectArgUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(1, mesh->GetIndirectCountUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(2, mesh->GetFalseNegativeUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(3, mesh->GetFalseNegativeCountUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(4, DrawCountUAV_.GetDescInfo().cpuHandle);

				for (sl12::u32 i = 0; i < submeshCount; ++i)
				{
					SceneSubmesh* submesh = mesh->GetSubmesh(i);
					descSet_.SetCsCbv(3, submesh->GetMeshletCBV()->GetDescInfo().cpuHandle);
					descSet_.SetCsSrv(0, submesh->GetMeshletBoundsBV()->GetDescInfo().cpuHandle);
					descSet_.SetCsSrv(1, submesh->GetMeshletDrawInfoBV()->GetDescInfo().cpuHandle);

					UINT dispatchCount = (submesh->GetMeshletData().meshletCount + 31) / 32;

					d3dCmdList->SetPipelineState(Cull1stPhasePSO_.GetPSO());
					pCmdList->SetComputeRootSignatureAndDescriptorSet(&Cull1stPhaseRS_, &descSet_);
					d3dCmdList->Dispatch(dispatchCount, 1, 1);
				}

				pCmdList->TransitionBarrier(mesh->GetIndirectArgBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
				pCmdList->TransitionBarrier(mesh->GetIndirectCountBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
			}
		}

		// set render target.
		{
			auto&& dsv = curGBuffer.depthDSV.GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(0, nullptr, false, &dsv);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = kScreenWidth;
			vp.Height = kScreenHeight;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = kScreenWidth;
			rect.bottom = kScreenHeight;
			d3dCmdList->RSSetScissorRects(1, &rect);
		}

		// 1st phase depth pre pass.
		{
			auto myPso = &PrePassPSO_;
			auto myRS = &PrePassRS_;

			// PSO設定
			d3dCmdList->SetPipelineState(myPso->GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetVsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, anisoSampler_.GetDescInfo().cpuHandle);

			auto RenderMesh = [&](const sl12::ResourceItemMesh* pMesh, sl12::ConstantBufferView* pMeshCBV, SceneMesh* pSceneMesh)
			{
				descSet_.SetVsCbv(1, pMeshCBV->GetDescInfo().cpuHandle);

				auto&& submeshes = pMesh->GetSubmeshes();
				auto submesh_count = submeshes.size();
				for (int i = 0; i < submesh_count; i++)
				{
					auto&& submesh = submeshes[i];
					auto sceneSubmesh = pSceneMesh->GetSubmesh(i);

					auto&& material = pMesh->GetMaterials()[submesh.materialIndex];
					auto bc_tex_res = const_cast<sl12::ResourceItemTexture*>(material.baseColorTex.GetItem<sl12::ResourceItemTexture>());
					auto&& base_color_srv = bc_tex_res->GetTextureView();

					descSet_.SetPsSrv(0, base_color_srv.GetDescInfo().cpuHandle);
					pCmdList->SetGraphicsRootSignatureAndDescriptorSet(myRS, &descSet_);

					const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
						submesh.positionVBV.GetView(),
						submesh.texcoordVBV.GetView(),
					};
					d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

					auto&& ibv = submesh.indexBV.GetView();
					d3dCmdList->IASetIndexBuffer(&ibv);

					auto&& meshletData = sceneSubmesh->GetMeshletData();
					d3dCmdList->ExecuteIndirect(
						commandSig_,																	// command signature
						meshletData.meshletCount,														// Meshlet最大数
						pSceneMesh->GetIndirectArgBuffer()->GetResourceDep(),							// indirectコマンドの変数バッファ
						sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshletData.indirectArg1stIndexOffset,	// indirectコマンドの変数バッファの先頭オフセット
						pSceneMesh->GetIndirectCountBuffer()->GetResourceDep(),							// 実際の発行回数を収めたカウントバッファ
						meshletData.indirectCount1stByteOffset);										// カウントバッファの先頭オフセット
				}
			};

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			RenderMesh(hMeshRes_.GetItem<sl12::ResourceItemMesh>(), &glbMeshCBV_, sponzaMesh_.get());
		}

		// render HiZ lambda.
		auto RenderHiZ = [&]()
		{
			sl12::TextureView* pSrcDepth = &curGBuffer.depthSRV;
			auto myPso = &ReduceDepth1stPSO_;
			auto myRS = &ReduceDepth1stRS_;
			int width = kScreenWidth / 2;
			int height = kScreenHeight / 2;
			for (int i = 0; i < HIZ_MIP_LEVEL; i++)
			{
				pCmdList->TransitionBarrier(&HiZTex_, i, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

				// set render target.
				D3D12_CPU_DESCRIPTOR_HANDLE rtv[] = {
					HiZSubRtvs_[i].GetDescInfo().cpuHandle,
				};
				d3dCmdList->OMSetRenderTargets(ARRAYSIZE(rtv), rtv, false, nullptr);

				D3D12_VIEWPORT vp;
				vp.TopLeftX = vp.TopLeftY = 0.0f;
				vp.Width = (float)width;
				vp.Height = (float)height;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				d3dCmdList->RSSetViewports(1, &vp);

				D3D12_RECT rect;
				rect.left = rect.top = 0;
				rect.right = width;
				rect.bottom = height;
				d3dCmdList->RSSetScissorRects(1, &rect);

				d3dCmdList->SetPipelineState(myPso->GetPSO());
				descSet_.Reset();
				descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(0, pSrcDepth->GetDescInfo().cpuHandle);

				pCmdList->SetMeshRootSignatureAndDescriptorSet(myRS, &descSet_);
				d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d3dCmdList->DrawInstanced(3, 1, 0, 0);

				pCmdList->TransitionBarrier(&HiZTex_, i, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);

				pSrcDepth = &HiZSubSrvs_[i];
				myPso = &ReduceDepth2ndPSO_;
				myRS = &ReduceDepth2ndRS_;
			}
		};

		// if occlusion culling enabled, execute 2nd phase culling.
		if (isOcclusionCulling_ && !isOcclusionReset_ && !isFreezeCull_)
		{
			// render HiZ
			pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
			RenderHiZ();
			pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			// 2nd phase culling.
			{
				descSet_.Reset();
				descSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetCsCbv(1, frustumCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetCsSrv(2, HiZSrv_.GetDescInfo().cpuHandle);

				SceneMesh* pSceneMeshes[] = {
					sponzaMesh_.get(),
				};
				for (auto mesh : pSceneMeshes)
				{
					sl12::u32 submeshCount = mesh->GetSubmeshCount();
					pCmdList->TransitionBarrier(mesh->GetIndirectArgBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					pCmdList->TransitionBarrier(mesh->GetIndirectCountBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					// cull current depth occlusion.
					descSet_.SetCsCbv(2, glbMeshCBV_.GetDescInfo().cpuHandle);
					descSet_.SetCsUav(0, mesh->GetIndirectArgUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(1, mesh->GetIndirectCountUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(2, mesh->GetFalseNegativeUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(3, mesh->GetFalseNegativeCountUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(4, DrawCountUAV_.GetDescInfo().cpuHandle);

					for (sl12::u32 i = 0; i < submeshCount; ++i)
					{
						SceneSubmesh* submesh = mesh->GetSubmesh(i);
						descSet_.SetCsCbv(3, submesh->GetMeshletCBV()->GetDescInfo().cpuHandle);
						descSet_.SetCsSrv(0, submesh->GetMeshletBoundsBV()->GetDescInfo().cpuHandle);
						descSet_.SetCsSrv(1, submesh->GetMeshletDrawInfoBV()->GetDescInfo().cpuHandle);

						UINT dispatchCount = (submesh->GetMeshletData().meshletCount + 31) / 32;

						d3dCmdList->SetPipelineState(Cull2ndPhasePSO_.GetPSO());
						pCmdList->SetComputeRootSignatureAndDescriptorSet(&Cull2ndPhaseRS_, &descSet_);
						d3dCmdList->Dispatch(dispatchCount, 1, 1);
					}

					pCmdList->TransitionBarrier(mesh->GetIndirectArgBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
					pCmdList->TransitionBarrier(mesh->GetIndirectCountBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
				}
			}

			// set render target.
			{
				auto&& dsv = curGBuffer.depthDSV.GetDescInfo().cpuHandle;
				d3dCmdList->OMSetRenderTargets(0, nullptr, false, &dsv);

				D3D12_VIEWPORT vp;
				vp.TopLeftX = vp.TopLeftY = 0.0f;
				vp.Width = kScreenWidth;
				vp.Height = kScreenHeight;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				d3dCmdList->RSSetViewports(1, &vp);

				D3D12_RECT rect;
				rect.left = rect.top = 0;
				rect.right = kScreenWidth;
				rect.bottom = kScreenHeight;
				d3dCmdList->RSSetScissorRects(1, &rect);
			}

			// 2nd phase depth pre pass.
			{
				auto myPso = &PrePassPSO_;
				auto myRS = &PrePassRS_;

				// PSO設定
				d3dCmdList->SetPipelineState(myPso->GetPSO());

				// 基本Descriptor設定
				descSet_.Reset();
				descSet_.SetVsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetPsSampler(0, anisoSampler_.GetDescInfo().cpuHandle);

				auto RenderMesh = [&](const sl12::ResourceItemMesh* pMesh, sl12::ConstantBufferView* pMeshCBV, SceneMesh* pSceneMesh)
				{
					descSet_.SetVsCbv(1, pMeshCBV->GetDescInfo().cpuHandle);

					auto&& submeshes = pMesh->GetSubmeshes();
					auto submesh_count = submeshes.size();
					for (int i = 0; i < submesh_count; i++)
					{
						auto&& submesh = submeshes[i];
						auto sceneSubmesh = pSceneMesh->GetSubmesh(i);

						auto&& material = pMesh->GetMaterials()[submesh.materialIndex];
						auto bc_tex_res = const_cast<sl12::ResourceItemTexture*>(material.baseColorTex.GetItem<sl12::ResourceItemTexture>());
						auto&& base_color_srv = bc_tex_res->GetTextureView();

						descSet_.SetPsSrv(0, base_color_srv.GetDescInfo().cpuHandle);
						pCmdList->SetGraphicsRootSignatureAndDescriptorSet(myRS, &descSet_);

						const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
							submesh.positionVBV.GetView(),
							submesh.texcoordVBV.GetView(),
						};
						d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

						auto&& ibv = submesh.indexBV.GetView();
						d3dCmdList->IASetIndexBuffer(&ibv);

						auto&& meshletData = sceneSubmesh->GetMeshletData();
						d3dCmdList->ExecuteIndirect(
							commandSig_,																	// command signature
							meshletData.meshletCount,														// Meshlet最大数
							pSceneMesh->GetIndirectArgBuffer()->GetResourceDep(),							// indirectコマンドの変数バッファ
							sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshletData.indirectArg2ndIndexOffset,	// indirectコマンドの変数バッファの先頭オフセット
							pSceneMesh->GetIndirectCountBuffer()->GetResourceDep(),							// 実際の発行回数を収めたカウントバッファ
							meshletData.indirectCount2ndByteOffset);										// カウントバッファの先頭オフセット
					}
				};

				// DrawCall
				d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				RenderMesh(hMeshRes_.GetItem<sl12::ResourceItemMesh>(), &glbMeshCBV_, sponzaMesh_.get());
			}
		}

		// copy draw count.
		{
			pCmdList->TransitionBarrier(&DrawCountBuffer_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
			d3dCmdList->CopyResource(DrawCountReadbacks_[frameIndex].GetResourceDep(), DrawCountBuffer_.GetResourceDep());
			pCmdList->TransitionBarrier(&DrawCountBuffer_, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		// set render target.
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtv[] = {
				curGBuffer.gbufferRTV[0].GetDescInfo().cpuHandle,
				curGBuffer.gbufferRTV[1].GetDescInfo().cpuHandle,
				curGBuffer.gbufferRTV[2].GetDescInfo().cpuHandle,
			};
			auto&& dsv = curGBuffer.depthDSV.GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(ARRAYSIZE(rtv), rtv, false, &dsv);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = kScreenWidth;
			vp.Height = kScreenHeight;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = kScreenWidth;
			rect.bottom = kScreenHeight;
			d3dCmdList->RSSetScissorRects(1, &rect);
		}

		// gbuffer pass.
		{
			auto myPso = &GBufferPSO_;
			auto myRS = &GBufferRS_;

			// PSO設定
			d3dCmdList->SetPipelineState(myPso->GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetVsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(1, materialCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, anisoSampler_.GetDescInfo().cpuHandle);

			auto RenderMesh = [&](const sl12::ResourceItemMesh* pMesh, sl12::ConstantBufferView* pMeshCBV, SceneMesh* pSceneMesh)
			{
				descSet_.SetVsCbv(1, pMeshCBV->GetDescInfo().cpuHandle);

				auto&& submeshes = pMesh->GetSubmeshes();
				auto submesh_count = submeshes.size();
				for (int i = 0; i < submesh_count; i++)
				{
					auto&& submesh = submeshes[i];
					auto sceneSubmesh = pSceneMesh->GetSubmesh(i);

					auto&& material = pMesh->GetMaterials()[submesh.materialIndex];
					auto bc_tex_res = const_cast<sl12::ResourceItemTexture*>(material.baseColorTex.GetItem<sl12::ResourceItemTexture>());
					auto n_tex_res = const_cast<sl12::ResourceItemTexture*>(material.normalTex.GetItem<sl12::ResourceItemTexture>());
					auto orm_tex_res = const_cast<sl12::ResourceItemTexture*>(material.ormTex.GetItem<sl12::ResourceItemTexture>());
					auto&& base_color_srv = bc_tex_res->GetTextureView();
					auto&& normal_srv = n_tex_res->GetTextureView();
					auto&& orm_srv = orm_tex_res->GetTextureView();

					descSet_.SetPsSrv(0, base_color_srv.GetDescInfo().cpuHandle);
					descSet_.SetPsSrv(1, normal_srv.GetDescInfo().cpuHandle);
					descSet_.SetPsSrv(2, orm_srv.GetDescInfo().cpuHandle);
					pCmdList->SetGraphicsRootSignatureAndDescriptorSet(myRS, &descSet_);

					const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
						submesh.positionVBV.GetView(),
						submesh.normalVBV.GetView(),
						submesh.tangentVBV.GetView(),
						submesh.texcoordVBV.GetView(),
					};
					d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

					auto&& ibv = submesh.indexBV.GetView();
					d3dCmdList->IASetIndexBuffer(&ibv);

#if 0
					d3dCmdList->DrawIndexedInstanced(submesh.indexCount, 1, 0, 0, 0);
#else
					auto&& meshletData = sceneSubmesh->GetMeshletData();
					d3dCmdList->ExecuteIndirect(
						commandSig_,																	// command signature
						meshletData.meshletCount,														// Meshlet最大数
						pSceneMesh->GetIndirectArgBuffer()->GetResourceDep(),							// indirectコマンドの変数バッファ
						sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshletData.indirectArg1stIndexOffset,	// indirectコマンドの変数バッファの先頭オフセット
						pSceneMesh->GetIndirectCountBuffer()->GetResourceDep(),							// 実際の発行回数を収めたカウントバッファ
						meshletData.indirectCount1stByteOffset);										// カウントバッファの先頭オフセット
#endif
				}
			};

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			RenderMesh(hMeshRes_.GetItem<sl12::ResourceItemMesh>(), &glbMeshCBV_, sponzaMesh_.get());
		}

		// リソースバリア
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[1], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[2], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

		// render HiZ
		RenderHiZ();

		// raytrace shadow.
		{
			pCmdList->TransitionBarrier(&rtShadowResult_.tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// デスクリプタを設定
			rtGlobalDescSet_.Reset();
			rtGlobalDescSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(1, curGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(2, curGBuffer.depthSRV.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(0, rtShadowResult_.uav.GetDescInfo().cpuHandle);

			// コピーしつつコマンドリストに積む
			D3D12_GPU_VIRTUAL_ADDRESS as_address[] = {
				rtTopAS_.GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress(),
			};
			pCmdList->SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, &rtDescMan_, as_address, ARRAYSIZE(as_address));

			// レイトレースを実行
			D3D12_DISPATCH_RAYS_DESC desc{};
			desc.HitGroupTable.StartAddress = rtDirectShadowHGTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.HitGroupTable.SizeInBytes = rtDirectShadowHGTable_.GetSize();
			desc.HitGroupTable.StrideInBytes = rtShaderRecordSize_;
			desc.MissShaderTable.StartAddress = rtDirectShadowMSTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.MissShaderTable.SizeInBytes = rtDirectShadowMSTable_.GetSize();
			desc.MissShaderTable.StrideInBytes = rtShaderRecordSize_;
			desc.RayGenerationShaderRecord.StartAddress = rtDirectShadowRGSTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = rtDirectShadowRGSTable_.GetSize();
			desc.Width = kScreenWidth;
			desc.Height = kScreenHeight;
			desc.Depth = 1;
			pCmdList->GetDxrCommandList()->SetPipelineState1(rtDirectShadowPSO_.GetPSO());
			pCmdList->GetDxrCommandList()->DispatchRays(&desc);

			pCmdList->TransitionBarrier(&rtShadowResult_.tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		// コマンドリスト変更
		pCmdList = &litCmdList;
		d3dCmdList = pCmdList->GetLatestCommandList();

		pCmdList->SetDescriptorHeapDirty();

		// lighting.
		{
			pCmdList->TransitionBarrier(&clusterInfoBuffer_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(&accumTex_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			descSet_.Reset();
			descSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetCsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(0, curGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(1, curGBuffer.gbufferSRV[1].GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(2, curGBuffer.gbufferSRV[2].GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(3, curGBuffer.depthSRV.GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(4, rtShadowResult_.srv.GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(5, pointLightsPosSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(6, pointLightsColorSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(7, clusterInfoSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsUav(0, accumUAV_.GetDescInfo().cpuHandle);

			d3dCmdList->SetPipelineState(LightingPSO_.GetPSO());
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&LightingRS_, &descSet_);
			UINT dx = (kScreenWidth + 7) / 8;
			UINT dy = (kScreenHeight + 7) / 8;
			d3dCmdList->Dispatch(dx, dy, 1);
		}

		// set render target.
		{
			auto&& rtv = swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = kScreenWidth;
			vp.Height = kScreenHeight;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = kScreenWidth;
			rect.bottom = kScreenHeight;
			d3dCmdList->RSSetScissorRects(1, &rect);
		}

		// to LDR
		{
			pCmdList->TransitionBarrier(&accumTex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			// PSO設定
			d3dCmdList->SetPipelineState(ToLdrPSO_.GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetPsSrv(0, accumSRV_.GetDescInfo().cpuHandle);

			pCmdList->SetMeshRootSignatureAndDescriptorSet(&ToLdrRS_, &descSet_);

			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3dCmdList->DrawInstanced(3, 1, 0, 0);
		}

		ImGui::Render();

		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		gpuTimestamp_[frameIndex].Query(pCmdList);
		gpuTimestamp_[frameIndex].Resolve(pCmdList);

		// コマンド終了と描画待ち
		zpreCmdLists_.Close();
		litCmdLists_.Close();
		device_.WaitDrawDone();

		// 次のフレームへ
		device_.Present(1);

		// enable occlusion culling.
		isOcclusionReset_ = false;

		// コマンド実行
		zpreCmdLists_.Execute();
		litCmdLists_.Execute();
	}

	void Finalize() override
	{
		// 描画待ち
		device_.WaitDrawDone();
		device_.Present(1);

		DestroyRaytracing();

		for (auto&& v : meshletComponents_) sl12::SafeDelete(v);
		meshletComponents_.clear();

		anisoSampler_.Destroy();

		glbMeshCBV_.Destroy();
		glbMeshCB_.Destroy();

		for (auto&& v : sceneCBVs_) v.Destroy();
		for (auto&& v : sceneCBs_) v.Destroy();

		for (auto&& v : lightCBVs_) v.Destroy();
		for (auto&& v : lightCBs_) v.Destroy();

		for (auto&& v : giCBVs_) v.Destroy();
		for (auto&& v : giCBs_) v.Destroy();

		for (auto&& v : materialCBVs_) v.Destroy();
		for (auto&& v : materialCBs_) v.Destroy();

		for (auto&& v : gpuTimestamp_) v.Destroy();

		gui_.Destroy();

		DestroyIndirectDrawParams();

		for (auto&& v : gbuffers_) v.Destroy();

		utilCmdList_.Destroy();
		litCmdLists_.Destroy();
		zpreCmdLists_.Destroy();

		sponzaMesh_.reset(nullptr);
		shaderManager_.Destroy();
		cbvCache_.Destroy();
	}

	int Input(UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_LBUTTONDOWN:
			inputData_.mouseButton |= sl12::MouseButton::Left;
			return 0;
		case WM_RBUTTONDOWN:
			inputData_.mouseButton |= sl12::MouseButton::Right;
			return 0;
		case WM_MBUTTONDOWN:
			inputData_.mouseButton |= sl12::MouseButton::Middle;
			return 0;
		case WM_LBUTTONUP:
			inputData_.mouseButton &= ~sl12::MouseButton::Left;
			return 0;
		case WM_RBUTTONUP:
			inputData_.mouseButton &= ~sl12::MouseButton::Right;
			return 0;
		case WM_MBUTTONUP:
			inputData_.mouseButton &= ~sl12::MouseButton::Middle;
			return 0;
		case WM_MOUSEMOVE:
			inputData_.mouseX = GET_X_LPARAM(lParam);
			inputData_.mouseY = GET_Y_LPARAM(lParam);
			return 0;
		}

		return 0;
	}

private:
	bool CreateSceneCB()
	{
		auto mtxWorldToView = DirectX::XMMatrixLookAtRH(
			DirectX::XMLoadFloat4(&camPos_),
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, kNearZ, kFarZ);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMStoreFloat4x4(&mtxWorldToView_, mtxWorldToView);
		mtxPrevWorldToView_ = mtxWorldToView_;

		DirectX::XMFLOAT4 lightDir = { 0.0f, -1.0f, 0.0f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		for (int i = 0; i < kBufferCount; i++)
		{
			if (!sceneCBs_[i].Initialize(&device_, sizeof(SceneCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}
			if (!sceneCBVs_[i].Initialize(&device_, &sceneCBs_[i]))
			{
				return false;
			}

			if (!lightCBs_[i].Initialize(&device_, sizeof(LightCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}
			if (!lightCBVs_[i].Initialize(&device_, &lightCBs_[i]))
			{
				return false;
			}

			if (!giCBs_[i].Initialize(&device_, sizeof(GlobalIlluminationCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}
			if (!giCBVs_[i].Initialize(&device_, &giCBs_[i]))
			{
				return false;
			}

			if (!materialCBs_[i].Initialize(&device_, sizeof(MaterialCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}
			if (!materialCBVs_[i].Initialize(&device_, &materialCBs_[i]))
			{
				return false;
			}
		}

		return true;
	}

	bool CreateMeshCB()
	{
		if (!glbMeshCB_.Initialize(&device_, sizeof(MeshCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
		{
			return false;
		}
		else
		{
			auto cb = reinterpret_cast<MeshCB*>(glbMeshCB_.Map(nullptr));
			DirectX::XMMATRIX m = DirectX::XMMatrixScaling(kSponzaScale, kSponzaScale, kSponzaScale);
			DirectX::XMStoreFloat4x4(&cb->mtxLocalToWorld, m);
			DirectX::XMStoreFloat4x4(&cb->mtxPrevLocalToWorld, m);
			glbMeshCB_.Unmap();

			if (!glbMeshCBV_.Initialize(&device_, &glbMeshCB_))
			{
				return false;
			}
		}

		return true;
	}

	void UpdateSceneCB(int frameIndex)
	{
		auto cp = DirectX::XMLoadFloat4(&camPos_);
		auto mtxWorldToView = DirectX::XMLoadFloat4x4(&mtxWorldToView_);
		auto mtxPrevWorldToView = DirectX::XMLoadFloat4x4(&mtxPrevWorldToView_);
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, kNearZ, kFarZ);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);
		auto mtxPrevWorldToClip = mtxPrevWorldToView * mtxViewToClip;
		auto mtxPrevClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxPrevWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 0.1f, -1.0f, 0.1f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { lightColor_[0] * lightPower_, lightColor_[1] * lightPower_, lightColor_[2] * lightPower_, 1.0f };

		{
			auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[frameIndex].Map(nullptr));
			DirectX::XMStoreFloat4x4(&cb->mtxWorldToProj, mtxWorldToClip);
			DirectX::XMStoreFloat4x4(&cb->mtxWorldToView, mtxWorldToView);
			DirectX::XMStoreFloat4x4(&cb->mtxViewToProj, mtxViewToClip);
			DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
			DirectX::XMStoreFloat4x4(&cb->mtxPrevWorldToProj, mtxPrevWorldToClip);
			DirectX::XMStoreFloat4x4(&cb->mtxPrevProjToWorld, mtxPrevClipToWorld);
			cb->screenInfo.x = kNearZ;
			cb->screenInfo.y = kFarZ;
			cb->screenInfo.z = kScreenWidth;
			cb->screenInfo.w = kScreenHeight;
			DirectX::XMStoreFloat4(&cb->camPos, cp);
			cb->isOcclusionCull = isOcclusionCulling_;
			sceneCBs_[frameIndex].Unmap();
		}

		{
			auto cb = reinterpret_cast<LightCB*>(lightCBs_[frameIndex].Map(nullptr));
			cb->lightDir = lightDir;
			cb->lightColor = lightColor;
			cb->skyPower = skyPower_;
			lightCBs_[frameIndex].Unmap();
		}

		{
			auto cb = reinterpret_cast<MaterialCB*>(materialCBs_[frameIndex].Map(nullptr));
			cb->roughnessRange = roughnessRange_;
			cb->metallicRange = metallicRange_;
			materialCBs_[frameIndex].Unmap();
		}

		loopCount_ = loopCount_ % MaxSample;

		if (!isFreezeCull_)
			DirectX::XMStoreFloat4x4(&mtxFrustumViewProj_, mtxWorldToClip);
		UpdateFrustumPlane(frameIndex, mtxFrustumViewProj_);
	}

	void ControlCamera()
	{
		// カメラ操作系入力
		const float kRotAngle = 1.0f;
		const float kMoveSpeed = 0.2f;
		camRotX_ = camRotY_ = camMoveForward_ = camMoveLeft_ = camMoveUp_ = 0.0f;
		if (GetKeyState('I') < 0)
		{
			isCameraMove_ = true;
			camRotX_ = kRotAngle;
		}
		else if (GetKeyState('K') < 0)
		{
			isCameraMove_ = true;
			camRotX_ = -kRotAngle;
		}
		if (GetKeyState('J') < 0)
		{
			isCameraMove_ = true;
			camRotY_ = kRotAngle;
		}
		else if (GetKeyState('L') < 0)
		{
			isCameraMove_ = true;
			camRotY_ = -kRotAngle;
		}
		if (GetKeyState('W') < 0)
		{
			isCameraMove_ = true;
			camMoveForward_ = kMoveSpeed;
		}
		else if (GetKeyState('S') < 0)
		{
			isCameraMove_ = true;
			camMoveForward_ = -kMoveSpeed;
		}
		if (GetKeyState('A') < 0)
		{
			isCameraMove_ = true;
			camMoveLeft_ = -kMoveSpeed;
		}
		else if (GetKeyState('D') < 0)
		{
			isCameraMove_ = true;
			camMoveLeft_ = kMoveSpeed;
		}
		if (GetKeyState('Q') < 0)
		{
			isCameraMove_ = true;
			camMoveUp_ = -kMoveSpeed;
		}
		else if (GetKeyState('E') < 0)
		{
			isCameraMove_ = true;
			camMoveUp_ = kMoveSpeed;
		}

		auto cp = DirectX::XMLoadFloat4(&camPos_);
		auto tp = DirectX::XMLoadFloat4(&tgtPos_);
		auto c_forward = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(tp, cp));
		auto c_right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(c_forward, DirectX::XMLoadFloat4(&upVec_)));
		auto mtxRot = DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationAxis(c_right, DirectX::XMConvertToRadians(camRotX_)), DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(camRotY_)));
		c_forward = DirectX::XMVector4Transform(c_forward, mtxRot);
		cp = DirectX::XMVectorAdd(cp, DirectX::XMVectorScale(c_forward, camMoveForward_));
		cp = DirectX::XMVectorAdd(cp, DirectX::XMVectorScale(c_right, camMoveLeft_));
		cp = DirectX::XMVectorAdd(cp, DirectX::XMVectorSet(0.0f, camMoveUp_, 0.0f, 0.0f));
		tp = DirectX::XMVectorAdd(cp, c_forward);
		DirectX::XMStoreFloat4(&camPos_, cp);
		DirectX::XMStoreFloat4(&tgtPos_, tp);

		mtxPrevWorldToView_ = mtxWorldToView_;
		auto mtx_world_to_view = DirectX::XMMatrixLookAtRH(
			cp,
			tp,
			DirectX::XMLoadFloat4(&upVec_));
		DirectX::XMStoreFloat4x4(&mtxWorldToView_, mtx_world_to_view);
	}

	bool CreateIndirectDrawParams()
	{
		// command signatureを生成する
		// NOTE: command signatureはGPUが生成する描画コマンドのデータの並び方を指定するオブジェクトです.
		//       multi draw indirectの場合はDraw系命令1つでOKで、この命令を複数連ねてExecuteIndirectでコマンドロードを実行します.
		{
			D3D12_INDIRECT_ARGUMENT_DESC args[1]{};
			args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
			D3D12_COMMAND_SIGNATURE_DESC desc{};
			desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
			desc.NumArgumentDescs = ARRAYSIZE(args);
			desc.pArgumentDescs = args;
			desc.NodeMask = 1;
			auto hr = device_.GetDeviceDep()->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&commandSig_));
			if (FAILED(hr))
			{
				return false;
			}
		}

		// 定数バッファを生成する
		for (int i = 0; i < ARRAYSIZE(frustumCBs_); i++)
		{
			if (!frustumCBs_[i].Initialize(&device_, sizeof(FrustumCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}

			if (!frustumCBVs_[i].Initialize(&device_, &frustumCBs_[i]))
			{
				return false;
			}
		}

		// Meshlet描画コンポーネントをサブメッシュ数分生成する
		{
			auto mesh_res = hMeshRes_.GetItem<sl12::ResourceItemMesh>();
			auto&& submeshes = mesh_res->GetSubmeshes();
			meshletComponents_.reserve(submeshes.size());
			for (auto&& submesh : submeshes)
			{
				MeshletRenderComponent* comp = new MeshletRenderComponent();
				if (!comp->Initialize(&device_, submesh.meshlets))
				{
					return false;
				}
				meshletComponents_.push_back(comp);
			}
		}

		return true;
	}

	void DestroyIndirectDrawParams()
	{
		sl12::SafeRelease(commandSig_);

		for (auto&& v : frustumCBs_) v.Destroy();
		for (auto&& v : frustumCBVs_) v.Destroy();
	}

	void UpdateFrustumPlane(int frameIndex, const DirectX::XMFLOAT4X4& mtxViewProj)
	{
		DirectX::XMFLOAT4 tmp_planes[6];

		// Left Frustum Plane
		// Add first column of the matrix to the fourth column
		tmp_planes[0].x = mtxViewProj._14 + mtxViewProj._11;
		tmp_planes[0].y = mtxViewProj._24 + mtxViewProj._21;
		tmp_planes[0].z = mtxViewProj._34 + mtxViewProj._31;
		tmp_planes[0].w = mtxViewProj._44 + mtxViewProj._41;

		// Right Frustum Plane
		// Subtract first column of matrix from the fourth column
		tmp_planes[1].x = mtxViewProj._14 - mtxViewProj._11;
		tmp_planes[1].y = mtxViewProj._24 - mtxViewProj._21;
		tmp_planes[1].z = mtxViewProj._34 - mtxViewProj._31;
		tmp_planes[1].w = mtxViewProj._44 - mtxViewProj._41;

		// Top Frustum Plane
		// Subtract second column of matrix from the fourth column
		tmp_planes[2].x = mtxViewProj._14 - mtxViewProj._12;
		tmp_planes[2].y = mtxViewProj._24 - mtxViewProj._22;
		tmp_planes[2].z = mtxViewProj._34 - mtxViewProj._32;
		tmp_planes[2].w = mtxViewProj._44 - mtxViewProj._42;

		// Bottom Frustum Plane
		// Add second column of the matrix to the fourth column
		tmp_planes[3].x = mtxViewProj._14 + mtxViewProj._12;
		tmp_planes[3].y = mtxViewProj._24 + mtxViewProj._22;
		tmp_planes[3].z = mtxViewProj._34 + mtxViewProj._32;
		tmp_planes[3].w = mtxViewProj._44 + mtxViewProj._42;

		// Near Frustum Plane
		// We could add the third column to the fourth column to get the near plane,
		// but we don't have to do this because the third column IS the near plane
		tmp_planes[4].x = mtxViewProj._13;
		tmp_planes[4].y = mtxViewProj._23;
		tmp_planes[4].z = mtxViewProj._33;
		tmp_planes[4].w = mtxViewProj._43;

		// Far Frustum Plane
		// Subtract third column of matrix from the fourth column
		tmp_planes[5].x = mtxViewProj._14 - mtxViewProj._13;
		tmp_planes[5].y = mtxViewProj._24 - mtxViewProj._23;
		tmp_planes[5].z = mtxViewProj._34 - mtxViewProj._33;
		tmp_planes[5].w = mtxViewProj._44 - mtxViewProj._43;

		// Normalize plane normals (A, B and C (xyz))
		// Also take note that planes face inward
		for (int i = 0; i < 6; ++i)
		{
			float length = sqrt((tmp_planes[i].x * tmp_planes[i].x) + (tmp_planes[i].y * tmp_planes[i].y) + (tmp_planes[i].z * tmp_planes[i].z));
			tmp_planes[i].x /= length;
			tmp_planes[i].y /= length;
			tmp_planes[i].z /= length;
			tmp_planes[i].w /= length;
		}

		auto p = reinterpret_cast<FrustumCB*>(frustumCBs_[frameIndex].Map(nullptr));
		memcpy(p->frustumPlanes, tmp_planes, sizeof(p->frustumPlanes));
		frustumCBs_[frameIndex].Unmap();
	}

	bool CreateBottomAS(sl12::CommandList* pCmdList, const sl12::ResourceItemMesh* pMeshItem, sl12::BottomAccelerationStructure* pBottomAS, bool isCompaction = false)
	{
		// Bottom ASの生成準備
		sl12::ResourceItemMesh* pLocalMesh = const_cast<sl12::ResourceItemMesh*>(pMeshItem);
		auto&& submeshes = pLocalMesh->GetSubmeshes();
		std::vector<sl12::GeometryStructureDesc> geoDescs(submeshes.size());
		for (int i = 0; i < submeshes.size(); i++)
		{
			auto&& submesh = submeshes[i];
			auto&& material = pLocalMesh->GetMaterials()[submesh.materialIndex];

			auto flags = material.isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			geoDescs[i].InitializeAsTriangle(
				flags,
				&pLocalMesh->GetPositionVB(),
				&pLocalMesh->GetIndexBuffer(),
				nullptr,
				pLocalMesh->GetPositionVB().GetStride(),
				submesh.vertexCount,
				submesh.positionVBV.GetBufferOffset(),
				DXGI_FORMAT_R32G32B32_FLOAT,
				submesh.indexCount,
				submesh.indexBV.GetBufferOffset(),
				DXGI_FORMAT_R32_UINT);
		}

		sl12::StructureInputDesc bottomInput{};
		auto buildFlag = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		if (isCompaction)
		{
			buildFlag |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
		}
		if (!bottomInput.InitializeAsBottom(&device_, geoDescs.data(), (UINT)submeshes.size(), buildFlag))
		{
			return false;
		}

		if (!pBottomAS->CreateBuffer(&device_, bottomInput.prebuildInfo.ResultDataMaxSizeInBytes, bottomInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}

		// コマンド発行
		if (!pBottomAS->Build(&device_, pCmdList, bottomInput))
		{
			return false;
		}

		return true;
	}

	bool CompactBottomAS(sl12::CommandList* pCmdList, sl12::BottomAccelerationStructure* pBottomAS)
	{
		return pBottomAS->CompactAS(&device_, pCmdList);
	}

	bool CreateTopAS(sl12::CommandList* pCmdList, sl12::TopInstanceDesc* pInstances, int instanceCount, sl12::TopAccelerationStructure* pTopAS)
	{
		sl12::StructureInputDesc topInput{};
		if (!topInput.InitializeAsTop(&device_, instanceCount, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		if (!pTopAS->CreateBuffer(&device_, topInput.prebuildInfo.ResultDataMaxSizeInBytes, topInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}
		if (!pTopAS->CreateInstanceBuffer(&device_, pInstances, instanceCount))
		{
			return false;
		}

		// コマンド発行
		if (!pTopAS->Build(pCmdList, topInput))
		{
			return false;
		}

		return true;
	}

	std::vector<const sl12::ResourceItemMesh*> GetBaseMeshes()
	{
		std::vector<const sl12::ResourceItemMesh*> ret;

		ret.push_back(hMeshRes_.GetItem<sl12::ResourceItemMesh>());

		return ret;
	}

	bool CreateCompactionBLAS(sl12::CommandList* pCmdList, std::vector<int>& TableOffsets)
	{
		// create bottom as.
		auto mesh_resources = GetBaseMeshes();
		int total_submesh_count = 0;
		for (int i = 0; i < mesh_resources.size(); i++)
		{
			sl12::BottomAccelerationStructure* bas = new sl12::BottomAccelerationStructure();
			if (!CreateBottomAS(pCmdList, mesh_resources[i], bas, true))
			{
				return false;
			}
			sl12::ConsolePrint("BLAS size before compaction : %lld bytes\n", bas->GetDxrBuffer().GetSize());

			rtBottomASs_.push_back(bas);
			TableOffsets.push_back(total_submesh_count);
			total_submesh_count += (int)mesh_resources[i]->GetSubmeshes().size();
		}
		TableOffsets.push_back(total_submesh_count);
	}

	bool CompactBLAS(sl12::CommandList* pCmdList, std::vector<sl12::Buffer*>& InfoBuffers)
	{
		int count = (int)rtBottomASs_.size();
		for (int i = 0; i < count; i++)
		{
			if (!CompactBottomAS(pCmdList, rtBottomASs_[i]))
			{
				return false;
			}
			sl12::ConsolePrint("BLAS size after compaction : %lld bytes\n", rtBottomASs_[i]->GetDxrBuffer().GetSize());
		}
		return true;
	}

	bool CreateTLAS(sl12::CommandList* pCmdList, std::vector<int>& TableOffsets)
	{
		// create top as.
		DirectX::XMFLOAT4X4 mtxSponza(
			kSponzaScale, 0, 0, 0,
			0, kSponzaScale, 0, 0,
			0, 0, kSponzaScale, 0,
			0, 0, 0, 1);
		sl12::TopInstanceDesc top_descs[1];
		top_descs[0].Initialize(mtxSponza, 0, 0xff, TableOffsets[0] * kRTMaterialTableCount, 0, rtBottomASs_[0]);
		if (!CreateTopAS(pCmdList, top_descs, ARRAYSIZE(top_descs), &rtTopAS_))
		{
			return false;
		}

		// initialize descriptor manager.
		if (!rtDescMan_.Initialize(&device_,
			2,		// Render Count
			1,		// AS Count
			4,		// Global CBV Count
			8,		// Global SRV Count
			4,		// Global UAV Count
			4,		// Global Sampler Count
			TableOffsets[TableOffsets.size() - 1]))
		{
			return false;
		}

		return true;
	}

	bool CreateAS(sl12::CommandList* pCmdList)
	{
		// create bottom as.
		auto mesh_resources = GetBaseMeshes();
		std::vector<int> table_offsets;
		int total_submesh_count = 0;
		for (int i = 0; i < mesh_resources.size(); i++)
		{
			sl12::BottomAccelerationStructure* bas = new sl12::BottomAccelerationStructure();
			if (!CreateBottomAS(pCmdList, mesh_resources[i], bas))
			{
				return false;
			}

			rtBottomASs_.push_back(bas);
			table_offsets.push_back(total_submesh_count);
			total_submesh_count += (int)mesh_resources[i]->GetSubmeshes().size();
		}

		// create top as.
		DirectX::XMFLOAT4X4 mtxSponza(
			kSponzaScale, 0, 0, 0,
			0, kSponzaScale, 0, 0,
			0, 0, kSponzaScale, 0,
			0, 0, 0, 1);
		sl12::TopInstanceDesc top_descs[1];
		top_descs[0].Initialize(mtxSponza, 0, 0xff, table_offsets[0] * kRTMaterialTableCount, 0, rtBottomASs_[0]);
		if (!CreateTopAS(pCmdList, top_descs, ARRAYSIZE(top_descs), &rtTopAS_))
		{
			return false;
		}

		// initialize descriptor manager.
		if (!rtDescMan_.Initialize(&device_,
			2,		// Render Count
			1,		// AS Count
			4,		// Global CBV Count
			8,		// Global SRV Count
			4,		// Global UAV Count
			4,		// Global Sampler Count
			total_submesh_count))
		{
			return false;
		}

		return true;
	}

	bool InitializeRaytracingPipeline()
	{
		// create root signature.
		// only one fixed root signature.
		if (!sl12::CreateRaytracingRootSignature(&device_,
			1,		// AS count
			4,		// Global CBV count
			8,		// Global SRV count
			4,		// Global UAV count
			4,		// Global Sampler count
			&rtGlobalRootSig_, &rtLocalRootSig_))
		{
			return false;
		}

		// create collection.
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			// export shader from library.
			auto shader = hShaders_[SHADER_OCCLUSION_LIB].GetShader();
			D3D12_EXPORT_DESC libExport[] = {
				{ kOcclusionCHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kOcclusionAHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(shader->GetData(), shader->GetSize(), libExport, ARRAYSIZE(libExport));

			// hit group.
			dxrDesc.AddHitGroup(kOcclusionOpacityHG, true, nullptr, kOcclusionCHS, nullptr);
			dxrDesc.AddHitGroup(kOcclusionMaskedHG, true, kOcclusionAHS, kOcclusionCHS, nullptr);

			// payload size and intersection attr size.
			dxrDesc.AddShaderConfig(16, sizeof(float) * 2);

			// global root signature.
			dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);

			// TraceRay recursive count.
			dxrDesc.AddRaytracinConfig(1);

			// local root signature.
			// if use only one root signature, do not need export association.
			dxrDesc.AddLocalRootSignatureAndExportAssociation(rtLocalRootSig_, nullptr, 0);

			// RaytracingPipelineConfigをリンク先で設定するため、このフラグが必要
			//dxrDesc.AddStateObjectConfig(D3D12_STATE_OBJECT_FLAG_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITIONS);

			// PSO生成
			if (!rtOcclusionCollection_.Initialize(&device_, dxrDesc, D3D12_STATE_OBJECT_TYPE_COLLECTION))
			{
				return false;
			}
		}
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			// export shader from library.
			auto shader = hShaders_[SHADER_MATERIAL_LIB].GetShader();
			D3D12_EXPORT_DESC libExport[] = {
				{ kMaterialCHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kMaterialAHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(shader->GetData(), shader->GetSize(), libExport, ARRAYSIZE(libExport));

			// hit group.
			dxrDesc.AddHitGroup(kMaterialOpacityHG, true, nullptr, kMaterialCHS, nullptr);
			dxrDesc.AddHitGroup(kMaterialMaskedHG, true, kMaterialAHS, kMaterialCHS, nullptr);

			// payload size and intersection attr size.
			dxrDesc.AddShaderConfig(16, sizeof(float) * 2);

			// global root signature.
			dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);

			// TraceRay recursive count.
			dxrDesc.AddRaytracinConfig(1);

			// local root signature.
			// if use only one root signature, do not need export association.
			dxrDesc.AddLocalRootSignatureAndExportAssociation(rtLocalRootSig_, nullptr, 0);

			// RaytracingPipelineConfigをリンク先で設定するため、このフラグが必要
			//dxrDesc.AddStateObjectConfig(D3D12_STATE_OBJECT_FLAG_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITIONS);

			// PSO生成
			sl12::CpuTimer time = sl12::CpuTimer::CurrentTime();
			if (!rtMaterialCollection_.Initialize(&device_, dxrDesc, D3D12_STATE_OBJECT_TYPE_COLLECTION))
			{
				return false;
			}
			time = sl12::CpuTimer::CurrentTime() - time;
			std::string str = std::string("Create Mat Collection : ") + std::to_string(time.ToMicroSecond()) + std::string("(us)\n");
			OutputDebugStringA(str.c_str());
		}

		// create pipeline state.
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			// export shader from library.
			auto shader = hShaders_[SHADER_DIRECT_SHADOW_LIB].GetShader();
			D3D12_EXPORT_DESC libExport[] = {
				{ kDirectShadowRGS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kDirectShadowMS,	nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(shader->GetData(), shader->GetSize(), libExport, ARRAYSIZE(libExport));

			// payload size and intersection attr size.
			dxrDesc.AddShaderConfig(16, sizeof(float) * 2);
			//dxrDesc.AddShaderConfig(sizeof(float) * 1, sizeof(float) * 2);

			// global root signature.
			dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);

			// TraceRay recursive count.
			dxrDesc.AddRaytracinConfig(1);

			// hit group collection.
			dxrDesc.AddExistingCollection(rtOcclusionCollection_.GetPSO(), nullptr, 0);

			// PSO生成
			if (!rtDirectShadowPSO_.Initialize(&device_, dxrDesc))
			{
				return false;
			}
		}

		return true;
	}

	bool InitializeRaytracingResource()
	{
		// create render targets.
		{
			sl12::TextureDesc desc{};
			desc.format = DXGI_FORMAT_R8_UNORM;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.depth = 1;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.mipLevels = 1;
			desc.sampleCount = 1;
			desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.isRenderTarget = true;
			desc.isUav = true;

			if (!rtShadowResult_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::TextureDesc desc{};
			desc.format = DXGI_FORMAT_R11G11B10_FLOAT;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.depth = 1;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.mipLevels = 1;
			desc.sampleCount = 1;
			desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.isRenderTarget = true;
			desc.isUav = true;

			if (!rtGIResult_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// create local shader resource table.
		struct LocalTable
		{
			D3D12_GPU_DESCRIPTOR_HANDLE	cbv;
			D3D12_GPU_DESCRIPTOR_HANDLE	srv;
			D3D12_GPU_DESCRIPTOR_HANDLE	uav;
			D3D12_GPU_DESCRIPTOR_HANDLE	sampler;
		};
		std::vector<LocalTable> material_table;
		std::vector<bool> opaque_table;
		auto view_desc_size = rtDescMan_.GetViewDescSize();
		auto sampler_desc_size = rtDescMan_.GetSamplerDescSize();
		auto local_handle_start = rtDescMan_.IncrementLocalHandleStart();
		auto FillTable = [&](const sl12::ResourceItemMesh* pMeshItem)
		{
			auto&& submeshes = pMeshItem->GetSubmeshes();
			for (int i = 0; i < submeshes.size(); i++)
			{
				auto&& submesh = submeshes[i];
				auto&& material = pMeshItem->GetMaterials()[submesh.materialIndex];
				auto pTexBC = material.baseColorTex.GetItem<sl12::ResourceItemTexture>();

				opaque_table.push_back(material.isOpaque);

				LocalTable table;

				// CBV
				table.cbv = local_handle_start.viewGpuHandle;

				// SRV
				D3D12_CPU_DESCRIPTOR_HANDLE srv[] = {
					submesh.indexView.GetDescInfo().cpuHandle,
					submesh.normalView.GetDescInfo().cpuHandle,
					submesh.texcoordView.GetDescInfo().cpuHandle,
					const_cast<sl12::ResourceItemTexture*>(pTexBC)->GetTextureView().GetDescInfo().cpuHandle,
				};
				sl12::u32 srv_cnt = ARRAYSIZE(srv);
				device_.GetDeviceDep()->CopyDescriptors(
					1, &local_handle_start.viewCpuHandle, &srv_cnt,
					srv_cnt, srv, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				table.srv = local_handle_start.viewGpuHandle;
				local_handle_start.viewCpuHandle.ptr += view_desc_size * srv_cnt;
				local_handle_start.viewGpuHandle.ptr += view_desc_size * srv_cnt;

				// UAVはなし
				table.uav = local_handle_start.viewGpuHandle;

				// Samplerは1つ
				D3D12_CPU_DESCRIPTOR_HANDLE sampler[] = {
					imageSampler_.GetDescInfo().cpuHandle,
				};
				sl12::u32 sampler_cnt = ARRAYSIZE(sampler);
				device_.GetDeviceDep()->CopyDescriptors(
					1, &local_handle_start.samplerCpuHandle, &sampler_cnt,
					sampler_cnt, sampler, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
				table.sampler = local_handle_start.samplerGpuHandle;
				local_handle_start.samplerCpuHandle.ptr += sampler_desc_size * sampler_cnt;
				local_handle_start.samplerGpuHandle.ptr += sampler_desc_size * sampler_cnt;

				material_table.push_back(table);
			}
		};
		auto mesh_resources = GetBaseMeshes();
		for (auto&& mesh : mesh_resources)
		{
			FillTable(mesh);
		}

		// create shader table.
		auto Align = [](UINT size, UINT align)
		{
			return ((size + align - 1) / align) * align;
		};
		UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
		UINT shaderRecordSize = Align(descHandleOffset + sizeof(LocalTable), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		rtShaderRecordSize_ = shaderRecordSize;

		auto GenShaderTable = [&](
			void* const * shaderIds,
			int tableCountPerMaterial,
			sl12::Buffer& buffer,
			int materialCount)
		{
			materialCount = (materialCount < 0) ? (int)material_table.size() : materialCount;
			if (!buffer.Initialize(&device_, shaderRecordSize * tableCountPerMaterial * materialCount, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer.Map(nullptr));
			for (int i = 0; i < materialCount; ++i)
			{
				for (int id = 0; id < tableCountPerMaterial; ++id)
				{
					auto start = p;

					memcpy(p, shaderIds[i * tableCountPerMaterial + id], shaderIdentifierSize);
					p += descHandleOffset;

					memcpy(p, &material_table[i], sizeof(LocalTable));

					p = start + shaderRecordSize;
				}
			}
			buffer.Unmap();

			return true;
		};
		// for DirectShadow.
		{
			void* rgs_identifier;
			void* ms_identifier;
			void* hg_identifier[2];
			{
				ID3D12StateObjectProperties* prop;
				rtDirectShadowPSO_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
				rgs_identifier = prop->GetShaderIdentifier(kDirectShadowRGS);
				ms_identifier = prop->GetShaderIdentifier(kDirectShadowMS);
				hg_identifier[0] = prop->GetShaderIdentifier(kOcclusionOpacityHG);
				hg_identifier[1] = prop->GetShaderIdentifier(kOcclusionMaskedHG);
				prop->Release();
			}
			std::vector<void*> hg_table;
			for (auto v : opaque_table)
			{
				if (v)
				{
					hg_table.push_back(hg_identifier[0]);
					hg_table.push_back(hg_identifier[0]);
				}
				else
				{
					hg_table.push_back(hg_identifier[1]);
					hg_table.push_back(hg_identifier[1]);
				}
			}
			if (!GenShaderTable(&rgs_identifier, 1, rtDirectShadowRGSTable_, 1))
			{
				return false;
			}
			if (!GenShaderTable(&ms_identifier, 1, rtDirectShadowMSTable_, 1))
			{
				return false;
			}
			if (!GenShaderTable(hg_table.data(), kRTMaterialTableCount, rtDirectShadowHGTable_, -1))
			{
				return false;
			}
		}

		return true;
	}

	void DestroyRaytracing()
	{
		rtDirectShadowRGSTable_.Destroy();
		rtDirectShadowMSTable_.Destroy();
		rtDirectShadowHGTable_.Destroy();

		rtGlobalRootSig_.Destroy();
		rtLocalRootSig_.Destroy();
		rtOcclusionCollection_.Destroy();
		rtMaterialCollection_.Destroy();
		rtDirectShadowPSO_.Destroy();

		rtDescMan_.Destroy();
		for (auto&& bas : rtBottomASs_) delete bas;
		rtBottomASs_.clear();
		rtTopAS_.Destroy();
	}

	bool CreatePointLights(
		const DirectX::XMFLOAT3& minPos, const DirectX::XMFLOAT3& maxPos,
		float minRadius, float maxRadius,
		float minIntensity, float maxIntensity)
	{
		static const sl12::u32 kPointLightCount = 128;

		if (!pointLightsPosBuffer_.Initialize(&device_, sizeof(PointLightPos) * kPointLightCount, sizeof(PointLightPos), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
		{
			return false;
		}
		if (!pointLightsColorBuffer_.Initialize(&device_, sizeof(PointLightColor) * kPointLightCount, sizeof(PointLightColor), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
		{
			return false;
		}
		if (!clusterInfoBuffer_.Initialize(&device_, sizeof(ClusterInfo) * CLUSTER_DIV_XY * CLUSTER_DIV_XY * CLUSTER_DIV_Z, sizeof(ClusterInfo), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, false, true))
		{
			return false;
		}

		pointLightsPosSRV_.Initialize(&device_, &pointLightsPosBuffer_, 0, kPointLightCount, sizeof(PointLightPos));
		pointLightsColorSRV_.Initialize(&device_, &pointLightsColorBuffer_, 0, kPointLightCount, sizeof(PointLightColor));
		clusterInfoSRV_.Initialize(&device_, &clusterInfoBuffer_, 0, CLUSTER_DIV_XY * CLUSTER_DIV_XY * CLUSTER_DIV_Z, sizeof(ClusterInfo));
		clusterInfoUAV_.Initialize(&device_, &clusterInfoBuffer_, 0, CLUSTER_DIV_XY * CLUSTER_DIV_XY * CLUSTER_DIV_Z, sizeof(ClusterInfo), 0);

		auto pos = (PointLightPos*)pointLightsPosBuffer_.Map(nullptr);
		auto color = (PointLightColor*)pointLightsColorBuffer_.Map(nullptr);
		for (sl12::u32 i = 0; i < kPointLightCount; i++, pos++, color++)
		{
			pos->posAndRadius.x = sl12::GlobalRandom.GetFValueRange(minPos.x, maxPos.x);
			pos->posAndRadius.y = sl12::GlobalRandom.GetFValueRange(minPos.y, maxPos.y);
			pos->posAndRadius.z = sl12::GlobalRandom.GetFValueRange(minPos.z, maxPos.z);
			pos->posAndRadius.w = sl12::GlobalRandom.GetFValueRange(minRadius, maxRadius);

			float intensity = sl12::GlobalRandom.GetFValueRange(minIntensity, maxIntensity);
			color->color.x = sl12::GlobalRandom.GetFValueRange(0.0f, intensity);
			color->color.y = sl12::GlobalRandom.GetFValueRange(0.0f, intensity);
			color->color.z = sl12::GlobalRandom.GetFValueRange(0.0f, intensity);
		}
		pointLightsPosBuffer_.Unmap();
		pointLightsColorBuffer_.Unmap();

		return true;
	}

	void DestroyPointLights()
	{
		pointLightsPosSRV_.Destroy();
		pointLightsColorSRV_.Destroy();
		clusterInfoSRV_.Destroy();
		clusterInfoUAV_.Destroy();

		pointLightsPosBuffer_.Destroy();
		pointLightsColorBuffer_.Destroy();
		clusterInfoBuffer_.Destroy();
	}

private:
	struct RaytracingResult
	{
		sl12::Texture				tex;
		sl12::TextureView			srv;
		sl12::RenderTargetView		rtv;
		sl12::UnorderedAccessView	uav;

		bool Initialize(sl12::Device* pDevice, const sl12::TextureDesc& desc)
		{
			if (!tex.Initialize(pDevice, desc))
			{
				return false;
			}
			if (!srv.Initialize(pDevice, &tex))
			{
				return false;
			}
			if (!rtv.Initialize(pDevice, &tex))
			{
				return false;
			}
			if (!uav.Initialize(pDevice, &tex))
			{
				return false;
			}

			return true;
		}

		void Destroy()
		{
			uav.Destroy();
			rtv.Destroy();
			srv.Destroy();
			tex.Destroy();
		}
	};	// struct RaytracingResult

	struct GBuffers
	{
		sl12::Texture				gbufferTex[3];
		sl12::TextureView			gbufferSRV[3];
		sl12::RenderTargetView		gbufferRTV[3];

		sl12::Texture				depthTex;
		sl12::TextureView			depthSRV;
		sl12::DepthStencilView		depthDSV;

		bool Initialize(sl12::Device* pDev, int width, int height)
		{
			{
				const DXGI_FORMAT kFormats[] = {
					DXGI_FORMAT_R10G10B10A2_UNORM,
					DXGI_FORMAT_R16G16B16A16_FLOAT,
					DXGI_FORMAT_R16G16B16A16_FLOAT,
				};

				sl12::TextureDesc desc;
				desc.dimension = sl12::TextureDimension::Texture2D;
				desc.width = width;
				desc.height = height;
				desc.mipLevels = 1;
				desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
				desc.sampleCount = 1;
				desc.clearColor[4] = { 0.0f };
				desc.clearDepth = 1.0f;
				desc.clearStencil = 0;
				desc.isRenderTarget = true;
				desc.isDepthBuffer = false;
				desc.isUav = false;

				for (int i = 0; i < ARRAYSIZE(gbufferTex); i++)
				{
					desc.format = kFormats[i];

					if (!gbufferTex[i].Initialize(pDev, desc))
					{
						return false;
					}

					if (!gbufferSRV[i].Initialize(pDev, &gbufferTex[i]))
					{
						return false;
					}

					if (!gbufferRTV[i].Initialize(pDev, &gbufferTex[i]))
					{
						return false;
					}
				}
			}

			// 深度バッファを生成
			{
				sl12::TextureDesc desc;
				desc.dimension = sl12::TextureDimension::Texture2D;
				desc.width = width;
				desc.height = height;
				desc.mipLevels = 1;
				desc.format = DXGI_FORMAT_D32_FLOAT;
				desc.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				desc.sampleCount = 1;
				desc.clearColor[4] = { 0.0f };
				desc.clearDepth = 1.0f;
				desc.clearStencil = 0;
				desc.isRenderTarget = false;
				desc.isDepthBuffer = true;
				desc.isUav = false;
				if (!depthTex.Initialize(pDev, desc))
				{
					return false;
				}

				if (!depthSRV.Initialize(pDev, &depthTex))
				{
					return false;
				}

				if (!depthDSV.Initialize(pDev, &depthTex))
				{
					return false;
				}
			}

			return true;
		}

		void Destroy()
		{
			for (int i = 0; i < ARRAYSIZE(gbufferTex); i++)
			{
				gbufferRTV[i].Destroy();
				gbufferSRV[i].Destroy();
				gbufferTex[i].Destroy();
			}
			depthDSV.Destroy();
			depthSRV.Destroy();
			depthTex.Destroy();
		}
	};	// struct GBuffers

	static const int kBufferCount = sl12::Swapchain::kMaxBuffer;

	struct CommandLists
	{
		sl12::CommandList	cmdLists[kBufferCount];
		int					index = 0;

		bool Initialize(sl12::Device* pDev, sl12::CommandQueue* pQueue)
		{
			for (auto&& v : cmdLists)
			{
				if (!v.Initialize(pDev, pQueue, true))
				{
					return false;
				}
			}
			index = 0;
			return true;
		}

		void Destroy()
		{
			for (auto&& v : cmdLists) v.Destroy();
		}

		sl12::CommandList& Reset()
		{
			index = (index + 1) % kBufferCount;
			auto&& ret = cmdLists[index];
			ret.Reset();
			return ret;
		}

		void Close()
		{
			cmdLists[index].Close();
		}

		void Execute()
		{
			cmdLists[index].Execute();
		}

		sl12::CommandQueue* GetParentQueue()
		{
			return cmdLists[index].GetParentQueue();
		}
	};	// struct CommandLists

private:
	CommandLists			zpreCmdLists_;
	CommandLists			litCmdLists_;
	sl12::CommandList		utilCmdList_;

	RaytracingResult					rtShadowResult_;
	RaytracingResult					rtGIResult_;
	sl12::RootSignature					rtGlobalRootSig_, rtLocalRootSig_;
	sl12::RaytracingDescriptorManager	rtDescMan_;
	sl12::DescriptorSet					rtGlobalDescSet_;
	sl12::DxrPipelineState				rtOcclusionCollection_;
	sl12::DxrPipelineState				rtMaterialCollection_;
	sl12::DxrPipelineState				rtDirectShadowPSO_;

	std::vector<sl12::BottomAccelerationStructure*>	rtBottomASs_;
	sl12::TopAccelerationStructure					rtTopAS_;

	sl12::u32				rtShaderRecordSize_;
	sl12::Buffer			rtDirectShadowRGSTable_;
	sl12::Buffer			rtDirectShadowMSTable_;
	sl12::Buffer			rtDirectShadowHGTable_;

	sl12::Sampler			imageSampler_;
	sl12::Sampler			anisoSampler_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer				lightCBs_[kBufferCount];
	sl12::ConstantBufferView	lightCBVs_[kBufferCount];

	sl12::Buffer				giCBs_[kBufferCount];
	sl12::ConstantBufferView	giCBVs_[kBufferCount];

	sl12::Buffer				materialCBs_[kBufferCount];
	sl12::ConstantBufferView	materialCBVs_[kBufferCount];

	sl12::Buffer				glbMeshCB_;
	sl12::ConstantBufferView	glbMeshCBV_;

	sl12::Texture				HiZTex_;
	sl12::TextureView			HiZSrv_;
	sl12::TextureView			HiZSubSrvs_[HIZ_MIP_LEVEL];
	sl12::RenderTargetView		HiZSubRtvs_[HIZ_MIP_LEVEL];

	sl12::Buffer				DrawCountBuffer_;
	sl12::UnorderedAccessView	DrawCountUAV_;
	sl12::Buffer				DrawCountReadbacks_[kBufferCount];

	// multi draw indirect関連
	std::vector< MeshletRenderComponent*>	meshletComponents_;
	sl12::Buffer					frustumCBs_[kBufferCount];
	sl12::ConstantBufferView		frustumCBVs_[kBufferCount];

	GBuffers					gbuffers_[kBufferCount];
	sl12::Texture				accumTex_;
	sl12::TextureView			accumSRV_;
	sl12::RenderTargetView		accumRTV_;
	sl12::UnorderedAccessView	accumUAV_;

	sl12::Buffer				pointLightsPosBuffer_;
	sl12::BufferView			pointLightsPosSRV_;
	sl12::Buffer				pointLightsColorBuffer_;
	sl12::BufferView			pointLightsColorSRV_;
	sl12::Buffer				clusterInfoBuffer_;
	sl12::BufferView			clusterInfoSRV_;
	sl12::UnorderedAccessView	clusterInfoUAV_;

	ID3D12CommandSignature*		commandSig_ = nullptr;

	sl12::RootSignature			PrePassRS_, GBufferRS_, ToLdrRS_, ReduceDepth1stRS_, ReduceDepth2ndRS_, ClusterCullRS_, ResetCullDataRS_, Cull1stPhaseRS_, Cull2ndPhaseRS_, LightingRS_;
	sl12::GraphicsPipelineState	PrePassPSO_, GBufferPSO_, ToLdrPSO_, ReduceDepth1stPSO_, ReduceDepth2ndPSO_;
	sl12::ComputePipelineState	ClusterCullPSO_, ResetCullDataPSO_, Cull1stPhasePSO_, Cull2ndPhasePSO_, LightingPSO_;

	sl12::DescriptorSet			descSet_;

	sl12::Gui				gui_;
	sl12::InputData			inputData_{};

	sl12::Timestamp			gpuTimestamp_[sl12::Swapchain::kMaxBuffer];

	DirectX::XMFLOAT4		camPos_ = { -5.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		upVec_ = { 0.0f, 1.0f, 0.0f, 0.0f };
	float					skyPower_ = 0.1f;
	float					lightColor_[3] = { 1.0f, 1.0f, 1.0f };
	float					lightPower_ = 1.0f;
	DirectX::XMFLOAT2		roughnessRange_ = { 0.0f, 1.0f };
	DirectX::XMFLOAT2		metallicRange_ = { 0.0f, 1.0f };
	uint32_t				loopCount_ = 0;
	bool					isClearTarget_ = true;
	float					giIntensity_ = 1.0f;
	sl12::u32				giSampleTotal_ = 0;
	int						giSampleCount_ = 1;

	DirectX::XMFLOAT4X4		mtxWorldToView_, mtxPrevWorldToView_;
	float					camRotX_ = 0.0f;
	float					camRotY_ = 0.0f;
	float					camMoveForward_ = 0.0f;
	float					camMoveLeft_ = 0.0f;
	float					camMoveUp_ = 0.0f;
	bool					isCameraMove_ = true;

	bool					isOcclusionCulling_ = true;
	bool					isOcclusionReset_ = true;
	bool					isFreezeCull_ = false;
	DirectX::XMFLOAT4X4		mtxFrustumViewProj_;
	DirectX::XMFLOAT4		frustumCamPos_;

	int		frameIndex_ = 0;

	sl12::ResourceLoader	resLoader_;
	sl12::ResourceHandle	hMeshRes_;
	sl12::ResourceHandle	hBlueNoiseRes_;

	sl12::ConstantBufferCache	cbvCache_;
	sl12::ShaderManager			shaderManager_;
	sl12::ShaderHandle			hShaders_[SHADER_MAX];
	std::unique_ptr<SceneMesh>	sponzaMesh_;

	int						sceneState_ = 0;		// 0:loading scene, 1:main scene
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
