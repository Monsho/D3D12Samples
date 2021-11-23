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

#include "sl12/scene_root.h"
#include "sl12/scene_mesh.h"
#include "sl12/render_command.h"
#include "sl12/bvh_manager.h"

#define A_CPU
#include "../shader/AMD/ffx_a.h"
#include "../shader/AMD/ffx_fsr1.h"

#include "../shader/NVIDIA/NIS_Config.h"

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
		"target_copy.p.hlsl",
		"AMD/taa.c.hlsl",
		"AMD/taa.c.hlsl",
		"AMD/fsr_easu.c.hlsl",
		"AMD/fsr_rcas.c.hlsl",
		"NVIDIA/nis_scaler.c.hlsl",
	};

	static const char* kShaderEntryPoints[] = 
	{
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"main",
		"first",
		"main",
		"main",
		"main",
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
		SHADER_TARGET_COPY_P,
		SHADER_TAA_C,
		SHADER_TAA_FIRST_C,
		SHADER_FSR_EASU_C,
		SHADER_FSR_RCAS_C,
		SHADER_NIS_SCALER_C,

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
		hSponzaRes_ = resLoader_.LoadRequest<sl12::ResourceItemMesh>("data/sponza/sponza.rmesh");
		hSuzanneRes_ = resLoader_.LoadRequest<sl12::ResourceItemMesh>("data/suzanne/suzanne.rmesh");

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
					kShaderEntryPoints[i],
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
		if (!gbuffers_.Initialize(&device_, kScreenWidth, kScreenHeight))
		{
			return false;
		}

		for (int i = 0; i < ARRAYSIZE(accumRTs_); i++)
		{
			if (!accumRTs_[i].Initialize(&device_, kScreenWidth, kScreenHeight, DXGI_FORMAT_R11G11B10_FLOAT, true))
			{
				return false;
			}
		}
		if (!accumTemp_.Initialize(&device_, kScreenWidth, kScreenHeight, DXGI_FORMAT_R11G11B10_FLOAT, true))
		{
			return false;
		}

		// create HiZ.
		if (!HiZ_.Initialize(&device_, kScreenWidth, kScreenHeight))
		{
			return false;
		}

		// create LDR target.
		if (!ldrTarget_.Initialize(&device_, kScreenWidth, kScreenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true))
		{
			return false;
		}

		// create FSR work buffer.
		if (!fsrTargets_[0].Initialize(&device_, kScreenWidth, kScreenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true))
		{
			return false;
		}
		if (!fsrTargets_[1].Initialize(&device_, kScreenWidth, kScreenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true))
		{
			return false;
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
			desc.MipLODBias = 0.0f;

			anisoSamplers_[0].Initialize(&device_, desc);

			desc.MipLODBias = -log2f(1440.0f / 1080.0f);
			anisoSamplers_[1].Initialize(&device_, desc);
			
			desc.MipLODBias = -log2f(1440.0f / 720.0f);
			anisoSamplers_[2].Initialize(&device_, desc);
		}
		{
			D3D12_SAMPLER_DESC desc{};
			desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

			clampPointSampler_.Initialize(&device_, desc);

			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

			clampLinearSampler_.Initialize(&device_, desc);
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
		if (!TargetCopyRS_.Initialize(&device_,
			hShaders_[SHADER_FULLSCREEN_VV].GetShader(),
			hShaders_[SHADER_TARGET_COPY_P].GetShader(),
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
		if (!TaaRS_.Initialize(&device_,
			hShaders_[SHADER_TAA_C].GetShader()))
		{
			return false;
		}
		if (!TaaFirstRS_.Initialize(&device_,
			hShaders_[SHADER_TAA_FIRST_C].GetShader()))
		{
			return false;
		}
		if (!FsrEasuRS_.Initialize(&device_,
			hShaders_[SHADER_FSR_EASU_C].GetShader()))
		{
			return false;
		}
		if (!FsrRcasRS_.Initialize(&device_,
			hShaders_[SHADER_FSR_RCAS_C].GetShader()))
		{
			return false;
		}
		if (!NisScalerRS_.Initialize(&device_,
			hShaders_[SHADER_NIS_SCALER_C].GetShader()))
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
			for (int i = 0; i < GBuffers::kMax; i++)
			{
				desc.rtvFormats[desc.numRTVs++] = gbuffers_.rts[i].tex->GetTextureDesc().format;
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
			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &TargetCopyRS_;
			desc.pVS = hShaders_[SHADER_FULLSCREEN_VV].GetShader();
			desc.pPS = hShaders_[SHADER_TARGET_COPY_P].GetShader();

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

			if (!TargetCopyPSO_.Initialize(&device_, desc))
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
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &TaaRS_;
			desc.pCS = hShaders_[SHADER_TAA_C].GetShader();

			if (!TaaPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &TaaFirstRS_;
			desc.pCS = hShaders_[SHADER_TAA_FIRST_C].GetShader();

			if (!TaaFirstPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &FsrEasuRS_;
			desc.pCS = hShaders_[SHADER_FSR_EASU_C].GetShader();

			if (!FsrEasuPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &FsrRcasRS_;
			desc.pCS = hShaders_[SHADER_FSR_RCAS_C].GetShader();

			if (!FsrRcasPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &NisScalerRS_;
			desc.pCS = hShaders_[SHADER_NIS_SCALER_C].GetShader();

			if (!NisScalerPSO_.Initialize(&device_, desc))
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

		// BVH Manager
		bvhManager_ = std::make_unique<sl12::BvhManager>(&device_);
		if (!bvhManager_)
		{
			return false;
		}

		// create NIS coeff.
		if (!CreateNisCoeffTex(&device_, &utilCmdList_))
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
			auto&& dsv = gbuffers_.depthDSV->GetDescInfo().cpuHandle;
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

			for (auto&& buff : InfoBuffers)
			{
				device_.KillObject(buff);
			}

			sceneRoot_ = std::make_unique<sl12::SceneRoot>();
			sponzaMesh_ = std::make_shared<sl12::SceneMesh>(&device_, hSponzaRes_.GetItem<sl12::ResourceItemMesh>());
			suzanneMesh_ = std::make_shared<sl12::SceneMesh>(&device_, hSuzanneRes_.GetItem<sl12::ResourceItemMesh>());

			DirectX::XMMATRIX m = DirectX::XMMatrixScaling(kSponzaScale, kSponzaScale, kSponzaScale);
			DirectX::XMFLOAT4X4 mf;
			DirectX::XMStoreFloat4x4(&mf, m);
			sponzaMesh_->SetMtxLocalToWorld(mf);

			m = DirectX::XMMatrixScaling(kSuzanneScale, kSuzanneScale, kSuzanneScale);
			DirectX::XMStoreFloat4x4(&mf, m);
			suzanneMesh_->SetMtxLocalToWorld(mf);

			sceneRoot_->AttachNode(sponzaMesh_);
			sceneRoot_->AttachNode(suzanneMesh_);
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
		auto&& currAccum = accumRTs_[frameIndex];
		auto&& prevAccum = accumRTs_[prevFrameIndex];

		// TEST: move suzanne.
		{
			static float sSuzanneAngle = 0.0f;

			DirectX::XMMATRIX ms = DirectX::XMMatrixScaling(kSuzanneScale, kSuzanneScale, kSuzanneScale);
			DirectX::XMMATRIX mt = DirectX::XMMatrixTranslation(0.0f, -5.0f, -5.0f);
			DirectX::XMMATRIX mr = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(sSuzanneAngle));
			DirectX::XMMATRIX m = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(ms, mt), mr);
			DirectX::XMFLOAT4X4 mf;
			DirectX::XMStoreFloat4x4(&mf, m);
			suzanneMesh_->SetMtxLocalToWorld(mf);

			sSuzanneAngle += 1.0f;
		}

		// カメラ操作
		ControlCamera();
		if (isCameraMove_)
		{
			giSampleTotal_ = 0;
			isCameraMove_ = false;
		}

		// gather mesh render commands.
		sl12::RenderCommandsList meshRenderCmds;
		sceneRoot_->GatherRenderCommands(&cbvCache_, meshRenderCmds);

		// add commands to BVH manager.
		for (auto&& cmd : meshRenderCmds)
		{
			if (cmd->GetType() == sl12::RenderCommandType::Mesh)
			{
				bvhManager_->AddGeometry(static_cast<sl12::MeshRenderCommand*>(cmd.get()));
			}
		}

		// GUI
		gui_.BeginNewFrame(&litCmdList, kScreenWidth, kScreenHeight, inputData_);
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
			const char* kRenderResItems[] = {
				"2560 x 1440",
				"1920 x 1080",
				"1280 x 720",
			};
			if (ImGui::Combo("Render Res", &currRenderRes_, kRenderResItems, ARRAYSIZE(kRenderResItems)))
			{
				// recreate render targets.
				const int kRenderSize[] = {
					2560, 1440,
					1920, 1080,
					1280, 720,
				};
				renderWidth_ = kRenderSize[currRenderRes_ * 2 + 0];
				renderHeight_ = kRenderSize[currRenderRes_ * 2 + 1];

				gbuffers_.Destroy();
				accumTemp_.Destroy();
				HiZ_.Destroy();
				ldrTarget_.Destroy();
				if (!gbuffers_.Initialize(&device_, renderWidth_, renderHeight_))
				{
					assert(!"Error: Recreate gbuffers.");
				}
				for (int i = 0; i < ARRAYSIZE(accumRTs_); i++)
				{
					accumRTs_[i].Destroy();
					if (!accumRTs_[i].Initialize(&device_, renderWidth_, renderHeight_, DXGI_FORMAT_R11G11B10_FLOAT, true))
					{
						assert(!"Error: Recreate accumRTs");
					}
				}
				if (!accumTemp_.Initialize(&device_, renderWidth_, renderHeight_, DXGI_FORMAT_R11G11B10_FLOAT, true))
				{
					assert(!"Error: Recreate accumTemp");
				}
				if (!HiZ_.Initialize(&device_, renderWidth_, renderHeight_))
				{
					assert(!"Error: Recreate HiZ");
				}
				if (!ldrTarget_.Initialize(&device_, renderWidth_, renderHeight_, DXGI_FORMAT_R8G8B8A8_UNORM, true))
				{
					assert(!"Error: Recreate LDR target");
				}
				{
					sl12::TextureDesc desc{};
					desc.format = DXGI_FORMAT_R8_UNORM;
					desc.width = renderWidth_;
					desc.height = renderHeight_;
					desc.depth = 1;
					desc.dimension = sl12::TextureDimension::Texture2D;
					desc.mipLevels = 1;
					desc.sampleCount = 1;
					desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
					desc.isRenderTarget = true;
					desc.isUav = true;

					rtShadowResult_.Destroy();
					if (!rtShadowResult_.Initialize(&device_, desc))
					{
						assert(!"Error: Recreate Shadow Result");
					}
				}
			}
			const char* kUpscaleItems[] = {
				"Bilinear",
				"FSR",
				"NIS",
			};
			if (ImGui::Combo("Upscaler", &currUpscaler_, kUpscaleItems, ARRAYSIZE(kUpscaleItems)))
			{
			}
			if (currUpscaler_ != 0)
			{
				if (ImGui::SliderFloat("Sharpness", &upscalerSharpness_, 0.0f, 1.0f))
				{
				}
			}
			if (ImGui::Checkbox("Use TAA", &useTAA_))
			{
				taaFirstRender_ = true;
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

		// determine rendering sampler.
		auto&& anisoSampler = (currUpscaler_ == 0)
			? anisoSamplers_[0]
			: anisoSamplers_[currRenderRes_];

		// update scene constant buffer.
		UpdateSceneCB(frameIndex, useTAA_);

		// build BVH.
		bvhManager_->BuildGeometry(pCmdList);
		sl12::RenderCommandsTempList tmpRenderCmds;
		auto pBvhScene = bvhManager_->BuildScene(pCmdList, meshRenderCmds, kRTMaterialTableCount, tmpRenderCmds);
		bvhManager_->CopyCompactionInfoOnGraphicsQueue(pCmdList);

		// create ray tracing shader table.
		bool bCreateRTShaderTableSuccess = CreateRayTracingShaderTable(tmpRenderCmds);
		assert(bCreateRTShaderTableSuccess);

		auto&& swapchain = device_.GetSwapchain();
		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle, color, 0, nullptr);
		}

		d3dCmdList->ClearDepthStencilView(gbuffers_.depthDSV->GetDescInfo().cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		pCmdList->TransitionBarrier(gbuffers_.rts[0].tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pCmdList->TransitionBarrier(gbuffers_.rts[1].tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pCmdList->TransitionBarrier(gbuffers_.rts[2].tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pCmdList->TransitionBarrier(gbuffers_.rts[3].tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

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
			descSet_.SetCsSrv(2, HiZ_.srv->GetDescInfo().cpuHandle);

			for (auto&& cmd : meshRenderCmds)
			{
				if (cmd->GetType() != sl12::RenderCommandType::Mesh)
				{
					continue;
				}
				sl12::MeshRenderCommand* mcmd = static_cast<sl12::MeshRenderCommand*>(cmd.get());
				sl12::SceneMesh* mesh = mcmd->GetParentMesh();

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
				descSet_.SetCsCbv(2, mcmd->GetCBView()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(0, mesh->GetIndirectArgUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(1, mesh->GetIndirectCountUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(2, mesh->GetFalseNegativeUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(3, mesh->GetFalseNegativeCountUAV()->GetDescInfo().cpuHandle);
				descSet_.SetCsUav(4, DrawCountUAV_.GetDescInfo().cpuHandle);

				for (auto&& sub_cmd : mcmd->GetSubmeshCommands())
				{
					sl12::SceneSubmesh* submesh = sub_cmd->GetParentSubmesh();
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
			auto&& dsv = gbuffers_.depthDSV->GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(0, nullptr, false, &dsv);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = (float)renderWidth_;
			vp.Height = (float)renderHeight_;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = renderWidth_;
			rect.bottom = renderHeight_;
			d3dCmdList->RSSetScissorRects(1, &rect);
		}

		// depth pre pass rendering function.
		auto RenderDepthPrePass = [&](int phaseIndex)
		{
			auto myPso = &PrePassPSO_;
			auto myRS = &PrePassRS_;

			// PSO設定
			d3dCmdList->SetPipelineState(myPso->GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetVsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, anisoSampler.GetDescInfo().cpuHandle);

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			for (auto&& cmd : meshRenderCmds)
			{
				if (cmd->GetType() != sl12::RenderCommandType::Mesh)
				{
					continue;
				}

				sl12::MeshRenderCommand* mcmd = static_cast<sl12::MeshRenderCommand*>(cmd.get());
				sl12::SceneMesh* pSceneMesh = mcmd->GetParentMesh();
				const sl12::ResourceItemMesh* pResMesh = pSceneMesh->GetParentResource();

				descSet_.SetVsCbv(1, mcmd->GetCBView()->GetDescInfo().cpuHandle);

				auto&& submeshes = pResMesh->GetSubmeshes();
				auto&& subCmds = mcmd->GetSubmeshCommands();
				auto submesh_count = submeshes.size();

				for (int i = 0; i < submesh_count; i++)
				{
					auto&& submesh = submeshes[i];
					auto sceneSubmesh = subCmds[i]->GetParentSubmesh();

					auto&& material = pResMesh->GetMaterials()[submesh.materialIndex];
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
					if (phaseIndex == 1)
					{
						d3dCmdList->ExecuteIndirect(
							commandSig_,																	// command signature
							meshletData.meshletCount,														// Meshlet最大数
							pSceneMesh->GetIndirectArgBuffer()->GetResourceDep(),							// indirectコマンドの変数バッファ
							sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshletData.indirectArg1stIndexOffset,	// indirectコマンドの変数バッファの先頭オフセット
							pSceneMesh->GetIndirectCountBuffer()->GetResourceDep(),							// 実際の発行回数を収めたカウントバッファ
							meshletData.indirectCount1stByteOffset);										// カウントバッファの先頭オフセット
					}
					else if (phaseIndex == 2)
					{
						d3dCmdList->ExecuteIndirect(
							commandSig_,																	// command signature
							meshletData.meshletCount,														// Meshlet最大数
							pSceneMesh->GetIndirectArgBuffer()->GetResourceDep(),							// indirectコマンドの変数バッファ
							sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshletData.indirectArg2ndIndexOffset,	// indirectコマンドの変数バッファの先頭オフセット
							pSceneMesh->GetIndirectCountBuffer()->GetResourceDep(),							// 実際の発行回数を収めたカウントバッファ
							meshletData.indirectCount2ndByteOffset);										// カウントバッファの先頭オフセット
					}
				}
			}
		};

		// 1st phase depth pre pass.
		RenderDepthPrePass(1);

		// render HiZ lambda.
		auto RenderHiZ = [&]()
		{
			sl12::TextureView* pSrcDepth = gbuffers_.depthSRV;
			auto myPso = &ReduceDepth1stPSO_;
			auto myRS = &ReduceDepth1stRS_;
			int width = renderWidth_ / 2;
			int height = renderHeight_ / 2;
			for (int i = 0; i < HIZ_MIP_LEVEL; i++)
			{
				pCmdList->TransitionBarrier(HiZ_.tex, i, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

				// set render target.
				D3D12_CPU_DESCRIPTOR_HANDLE rtv[] = {
					HiZ_.subRtvs[i]->GetDescInfo().cpuHandle,
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

				pCmdList->TransitionBarrier(HiZ_.tex, i, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);

				pSrcDepth = HiZ_.subSrvs[i];
				myPso = &ReduceDepth2ndPSO_;
				myRS = &ReduceDepth2ndRS_;
			}
		};

		// if occlusion culling enabled, execute 2nd phase culling.
		if (isOcclusionCulling_ && !isOcclusionReset_ && !isFreezeCull_)
		{
			// render HiZ
			pCmdList->TransitionBarrier(gbuffers_.depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
			RenderHiZ();
			pCmdList->TransitionBarrier(gbuffers_.depthTex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			// 2nd phase culling.
			{
				descSet_.Reset();
				descSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetCsCbv(1, frustumCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetCsSrv(2, HiZ_.srv->GetDescInfo().cpuHandle);

				for (auto&& cmd : meshRenderCmds)
				{
					if (cmd->GetType() != sl12::RenderCommandType::Mesh)
					{
						continue;
					}
					sl12::MeshRenderCommand* mcmd = static_cast<sl12::MeshRenderCommand*>(cmd.get());
					sl12::SceneMesh* mesh = mcmd->GetParentMesh();

					sl12::u32 submeshCount = mesh->GetSubmeshCount();
					pCmdList->TransitionBarrier(mesh->GetIndirectArgBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					pCmdList->TransitionBarrier(mesh->GetIndirectCountBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					// cull current depth occlusion.
					descSet_.SetCsCbv(2, mcmd->GetCBView()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(0, mesh->GetIndirectArgUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(1, mesh->GetIndirectCountUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(2, mesh->GetFalseNegativeUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(3, mesh->GetFalseNegativeCountUAV()->GetDescInfo().cpuHandle);
					descSet_.SetCsUav(4, DrawCountUAV_.GetDescInfo().cpuHandle);

					for (auto&& sub_cmd : mcmd->GetSubmeshCommands())
					{
						sl12::SceneSubmesh* submesh = sub_cmd->GetParentSubmesh();
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
				auto&& dsv = gbuffers_.depthDSV->GetDescInfo().cpuHandle;
				d3dCmdList->OMSetRenderTargets(0, nullptr, false, &dsv);

				D3D12_VIEWPORT vp;
				vp.TopLeftX = vp.TopLeftY = 0.0f;
				vp.Width = (float)renderWidth_;
				vp.Height = (float)renderHeight_;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				d3dCmdList->RSSetViewports(1, &vp);

				D3D12_RECT rect;
				rect.left = rect.top = 0;
				rect.right = renderWidth_;
				rect.bottom = renderHeight_;
				d3dCmdList->RSSetScissorRects(1, &rect);
			}

			// 2nd phase depth pre pass.
			RenderDepthPrePass(2);
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
				gbuffers_.rts[GBuffers::kWorldQuat].rtv->GetDescInfo().cpuHandle,
				gbuffers_.rts[GBuffers::kBaseColor].rtv->GetDescInfo().cpuHandle,
				gbuffers_.rts[GBuffers::kMetalRough].rtv->GetDescInfo().cpuHandle,
				gbuffers_.rts[GBuffers::kVelocity].rtv->GetDescInfo().cpuHandle,
			};
			auto&& dsv = gbuffers_.depthDSV->GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(ARRAYSIZE(rtv), rtv, false, &dsv);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = (float)renderWidth_;
			vp.Height = (float)renderHeight_;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = renderWidth_;
			rect.bottom = renderHeight_;
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
			descSet_.SetPsSampler(0, anisoSampler.GetDescInfo().cpuHandle);

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			for (auto&& cmd : meshRenderCmds)
			{
				if (cmd->GetType() != sl12::RenderCommandType::Mesh)
				{
					continue;
				}

				sl12::MeshRenderCommand* mcmd = static_cast<sl12::MeshRenderCommand*>(cmd.get());
				sl12::SceneMesh* pSceneMesh = mcmd->GetParentMesh();
				const sl12::ResourceItemMesh* pResMesh = pSceneMesh->GetParentResource();

				descSet_.SetVsCbv(1, mcmd->GetCBView()->GetDescInfo().cpuHandle);

				auto&& submeshes = pResMesh->GetSubmeshes();
				auto&& subCmds = mcmd->GetSubmeshCommands();
				auto submesh_count = submeshes.size();
				for (int i = 0; i < submesh_count; i++)
				{
					auto&& submesh = submeshes[i];
					auto sceneSubmesh = subCmds[i]->GetParentSubmesh();

					auto&& material = pResMesh->GetMaterials()[submesh.materialIndex];
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

					auto&& meshletData = sceneSubmesh->GetMeshletData();
					d3dCmdList->ExecuteIndirect(
						commandSig_,																	// command signature
						meshletData.meshletCount,														// Meshlet最大数
						pSceneMesh->GetIndirectArgBuffer()->GetResourceDep(),							// indirectコマンドの変数バッファ
						sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshletData.indirectArg1stIndexOffset,	// indirectコマンドの変数バッファの先頭オフセット
						pSceneMesh->GetIndirectCountBuffer()->GetResourceDep(),							// 実際の発行回数を収めたカウントバッファ
						meshletData.indirectCount1stByteOffset);										// カウントバッファの先頭オフセット
				}
			}
		}

		// リソースバリア
		pCmdList->TransitionBarrier(gbuffers_.rts[GBuffers::kWorldQuat].tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(gbuffers_.rts[GBuffers::kBaseColor].tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(gbuffers_.rts[GBuffers::kMetalRough].tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(gbuffers_.rts[GBuffers::kVelocity].tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(gbuffers_.depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

		// render HiZ
		RenderHiZ();

		// raytrace shadow.
		{
			pCmdList->TransitionBarrier(rtShadowResult_.tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// デスクリプタを設定
			rtGlobalDescSet_.Reset();
			rtGlobalDescSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(1, gbuffers_.rts[GBuffers::kWorldQuat].srv->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(2, gbuffers_.depthSRV->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(0, rtShadowResult_.uav->GetDescInfo().cpuHandle);

			// コピーしつつコマンドリストに積む
			D3D12_GPU_VIRTUAL_ADDRESS as_address[] = {
				pBvhScene->GetGPUAddress(),
			};
			pCmdList->SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, bvhDescMan_.get(), as_address, ARRAYSIZE(as_address));

			// レイトレースを実行
			D3D12_DISPATCH_RAYS_DESC desc{};
			desc.HitGroupTable.StartAddress = bvhDirectShadowHGTable_->GetResourceDep()->GetGPUVirtualAddress();
			desc.HitGroupTable.SizeInBytes = bvhDirectShadowHGTable_->GetSize();
			desc.HitGroupTable.StrideInBytes = bvhShaderRecordSize_;
			desc.MissShaderTable.StartAddress = bvhDirectShadowMSTable_->GetResourceDep()->GetGPUVirtualAddress();
			desc.MissShaderTable.SizeInBytes = bvhDirectShadowMSTable_->GetSize();
			desc.MissShaderTable.StrideInBytes = bvhShaderRecordSize_;
			desc.RayGenerationShaderRecord.StartAddress = bvhDirectShadowRGSTable_->GetResourceDep()->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = bvhDirectShadowRGSTable_->GetSize();
			desc.Width = renderWidth_;
			desc.Height = renderHeight_;
			desc.Depth = 1;
			pCmdList->GetDxrCommandList()->SetPipelineState1(rtDirectShadowPSO_.GetPSO());
			pCmdList->GetDxrCommandList()->DispatchRays(&desc);

			pCmdList->TransitionBarrier(rtShadowResult_.tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		// コマンドリスト変更
		pCmdList = &litCmdList;
		d3dCmdList = pCmdList->GetLatestCommandList();

		pCmdList->SetDescriptorHeapDirty();

		// lighting.
		{
			auto&& target = useTAA_ ? accumTemp_ : currAccum;

			pCmdList->TransitionBarrier(&clusterInfoBuffer_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(target.tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			descSet_.Reset();
			descSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetCsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(0, gbuffers_.rts[GBuffers::kWorldQuat].srv->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(1, gbuffers_.rts[GBuffers::kBaseColor].srv->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(2, gbuffers_.rts[GBuffers::kMetalRough].srv->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(3, gbuffers_.depthSRV->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(4, rtShadowResult_.srv->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(5, pointLightsPosSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(6, pointLightsColorSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(7, clusterInfoSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsUav(0, target.uav->GetDescInfo().cpuHandle);

			d3dCmdList->SetPipelineState(LightingPSO_.GetPSO());
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&LightingRS_, &descSet_);
			UINT dx = (renderWidth_ + 7) / 8;
			UINT dy = (renderHeight_ + 7) / 8;
			d3dCmdList->Dispatch(dx, dy, 1);
		}

		// TAA
		if (useTAA_)
		{
			pCmdList->TransitionBarrier(accumTemp_.tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(currAccum.tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			descSet_.Reset();
			descSet_.SetCsSrv(0, accumTemp_.srv->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(1, gbuffers_.depthSRV->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(2, prevAccum.srv->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(3, gbuffers_.rts[GBuffers::kVelocity].srv->GetDescInfo().cpuHandle);
			descSet_.SetCsUav(0, currAccum.uav->GetDescInfo().cpuHandle);
			descSet_.SetCsSampler(0, clampPointSampler_.GetDescInfo().cpuHandle);
			descSet_.SetCsSampler(1, clampPointSampler_.GetDescInfo().cpuHandle);
			descSet_.SetCsSampler(2, clampLinearSampler_.GetDescInfo().cpuHandle);
			descSet_.SetCsSampler(3, clampPointSampler_.GetDescInfo().cpuHandle);

			if (taaFirstRender_)
			{
				d3dCmdList->SetPipelineState(TaaFirstPSO_.GetPSO());
				pCmdList->SetComputeRootSignatureAndDescriptorSet(&TaaFirstRS_, &descSet_);
				taaFirstRender_ = false;
			}
			else
			{
				d3dCmdList->SetPipelineState(TaaPSO_.GetPSO());
				pCmdList->SetComputeRootSignatureAndDescriptorSet(&TaaRS_, &descSet_);
			}
			UINT dx = (renderWidth_ + 15) / 16;
			UINT dy = (renderHeight_ + 15) / 16;
			d3dCmdList->Dispatch(dx, dy, 1);
		}

		pCmdList->TransitionBarrier(gbuffers_.depthTex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		// set render target.
		{
			auto&& rtv = ldrTarget_.rtv->GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = (float)renderWidth_;
			vp.Height = (float)renderHeight_;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = renderWidth_;
			rect.bottom = renderHeight_;
			d3dCmdList->RSSetScissorRects(1, &rect);
		}

		// to LDR
		{
			pCmdList->TransitionBarrier(currAccum.tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(ldrTarget_.tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

			// PSO設定
			d3dCmdList->SetPipelineState(ToLdrPSO_.GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetPsSrv(0, currAccum.srv->GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, clampLinearSampler_.GetDescInfo().cpuHandle);

			pCmdList->SetMeshRootSignatureAndDescriptorSet(&ToLdrRS_, &descSet_);

			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3dCmdList->DrawInstanced(3, 1, 0, 0);
		}

		auto upscalerType = (currRenderRes_ == 0) ? 0 : currUpscaler_;

		// FSR
		if (upscalerType == 1)
		{
			// create constant buffer.
			struct
			{
				AU1 Const0[4];
				AU1 Const1[4];
				AU1 Const2[4];
				AU1 Const3[4];
			} fsrConst;
			struct
			{
				AU1 Const0[4];
			} rcasConst;
			FsrEasuCon(fsrConst.Const0, fsrConst.Const1, fsrConst.Const2, fsrConst.Const3,
				renderWidth_, renderHeight_,
				renderWidth_, renderHeight_,
				kScreenWidth, kScreenHeight);
			FsrRcasCon(rcasConst.Const0, 2.0f - (upscalerSharpness_ * 2.0f));

			auto hCB0 = cbvCache_.GetUnusedConstBuffer(sizeof(fsrConst), &fsrConst);
			auto hCB1 = cbvCache_.GetUnusedConstBuffer(sizeof(rcasConst), &rcasConst);

			// EASU
			pCmdList->TransitionBarrier(ldrTarget_.tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(fsrTargets_[0].tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			descSet_.Reset();
			descSet_.SetCsCbv(0, hCB0.GetCBV()->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(0, ldrTarget_.srv->GetDescInfo().cpuHandle);
			descSet_.SetCsUav(0, fsrTargets_[0].uav->GetDescInfo().cpuHandle);
			descSet_.SetCsSampler(0, clampLinearSampler_.GetDescInfo().cpuHandle);

			d3dCmdList->SetPipelineState(FsrEasuPSO_.GetPSO());
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&FsrEasuRS_, &descSet_);

			UINT dx = (kScreenWidth + 15) / 16;
			UINT dy = (kScreenHeight + 15) / 16;
			d3dCmdList->Dispatch(dx, dy, 1);

			// RCAS
			pCmdList->TransitionBarrier(fsrTargets_[0].tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(fsrTargets_[1].tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			descSet_.Reset();
			descSet_.SetCsCbv(0, hCB1.GetCBV()->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(0, fsrTargets_[0].srv->GetDescInfo().cpuHandle);
			descSet_.SetCsUav(0, fsrTargets_[1].uav->GetDescInfo().cpuHandle);
			descSet_.SetCsSampler(0, clampLinearSampler_.GetDescInfo().cpuHandle);

			d3dCmdList->SetPipelineState(FsrRcasPSO_.GetPSO());
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&FsrRcasRS_, &descSet_);

			d3dCmdList->Dispatch(dx, dy, 1);

			pCmdList->TransitionBarrier(fsrTargets_[1].tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}
		// NIS
		else if (upscalerType == 2)
		{
			// create constant buffer.
			NISConfig nisConfig;
			NVScalerUpdateConfig(nisConfig, upscalerSharpness_,
				0, 0,
				renderWidth_, renderHeight_,
				renderWidth_, renderHeight_,
				0, 0,
				kScreenWidth, kScreenHeight,
				kScreenWidth, kScreenHeight,
				NISHDRMode::None);

			auto hCB = cbvCache_.GetUnusedConstBuffer(sizeof(nisConfig), &nisConfig);

			pCmdList->TransitionBarrier(ldrTarget_.tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(fsrTargets_[1].tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			descSet_.Reset();
			descSet_.SetCsCbv(0, hCB.GetCBV()->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(0, ldrTarget_.srv->GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(1, nisCoeffScalerSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsSrv(2, nisCoeffUsmSRV_.GetDescInfo().cpuHandle);
			descSet_.SetCsUav(0, fsrTargets_[1].uav->GetDescInfo().cpuHandle);
			descSet_.SetCsSampler(0, clampLinearSampler_.GetDescInfo().cpuHandle);

			d3dCmdList->SetPipelineState(NisScalerPSO_.GetPSO());
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&NisScalerRS_, &descSet_);

			NISOptimizer optimizer(true, NISGPUArchitecture::NVIDIA_Generic);
			UINT dx = (kScreenWidth + optimizer.GetOptimalBlockWidth() - 1) / optimizer.GetOptimalBlockWidth();
			UINT dy = (kScreenHeight + optimizer.GetOptimalBlockHeight() - 1) / optimizer.GetOptimalBlockHeight();
			d3dCmdList->Dispatch(dx, dy, 1);

			pCmdList->TransitionBarrier(fsrTargets_[1].tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}
		else
		{
			pCmdList->TransitionBarrier(ldrTarget_.tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
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

		// to swapchain.
		{
			auto target = &ldrTarget_;
			if (upscalerType != 0)
			{
				target = &fsrTargets_[1];
			}

			// PSO設定
			d3dCmdList->SetPipelineState(TargetCopyPSO_.GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetPsSrv(0, target->srv->GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, clampLinearSampler_.GetDescInfo().cpuHandle);

			pCmdList->SetMeshRootSignatureAndDescriptorSet(&TargetCopyRS_, &descSet_);

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
		device_.KillObject(pBvhScene);		// delete TLAS.

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

		bvhManager_.reset();

		for (auto&& v : anisoSamplers_) v.Destroy();

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

		rtShadowResult_.Destroy();
		ldrTarget_.Destroy();
		HiZ_.Destroy();
		gbuffers_.Destroy();
		for (auto&& v : accumRTs_) v.Destroy();
		accumTemp_.Destroy();

		utilCmdList_.Destroy();
		litCmdLists_.Destroy();
		zpreCmdLists_.Destroy();

		sponzaMesh_.reset();
		suzanneMesh_.reset();
		sceneRoot_.reset();
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
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(60.0f), (float)renderWidth_ / (float)renderHeight_, kNearZ, kFarZ);
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

	DirectX::XMMATRIX GetJitterProjection(const DirectX::XMMATRIX& mtxViewToClip, sl12::u32 screenWidth, sl12::u32 screenHeight, sl12::u32& jitterFrame)
	{
		auto HaltonSequence = [](sl12::u32 index, sl12::u32 base)
		{
			float f = 1.0f, ret = 0.0f;

			for (sl12::u32 i = index; i > 0;)
			{
				f /= (float)base;
				ret = ret + f * (float)(i % base);
				i = (sl12::u32)(floorf((float)i / (float)base));
			}

			return ret;
		};

		jitterFrame = (jitterFrame + 1) % 16;

		float jitterX = 2.0f * HaltonSequence(jitterFrame + 1, 2) - 1.0f;
		float jitterY = 2.0f * HaltonSequence(jitterFrame + 1, 3) - 1.0f;

		jitterX /= (float)screenWidth;
		jitterY /= (float)screenHeight;

		DirectX::XMFLOAT4X4 temp;
		DirectX::XMStoreFloat4x4(&temp, mtxViewToClip);
		temp._31 = jitterX;
		temp._32 = jitterY;
		return DirectX::XMLoadFloat4x4(&temp);
	}

	void UpdateSceneCB(int frameIndex, bool useTAA)
	{
		auto cp = DirectX::XMLoadFloat4(&camPos_);
		auto mtxWorldToView = DirectX::XMLoadFloat4x4(&mtxWorldToView_);
		auto mtxPrevWorldToView = DirectX::XMLoadFloat4x4(&mtxPrevWorldToView_);
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(60.0f), (float)renderWidth_ / (float)renderHeight_, kNearZ, kFarZ);

		if (useTAA)
		{
			static sl12::u32 sJitterFrame = 0;
			mtxViewToClip = GetJitterProjection(mtxViewToClip, renderWidth_, renderHeight_, sJitterFrame);
		}

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
			cb->screenInfo.z = (float)renderWidth_;
			cb->screenInfo.w = (float)renderHeight_;
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

		ret.push_back(hSponzaRes_.GetItem<sl12::ResourceItemMesh>());
		ret.push_back(hSuzanneRes_.GetItem<sl12::ResourceItemMesh>());

		return ret;
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

		return true;
	}

	bool CreateRayTracingShaderTable(sl12::RenderCommandsTempList& tcmds)
	{
		// already created.
		if ((bvhDirectShadowRGSTable_ != nullptr)
			&& (bvhDirectShadowMSTable_ != nullptr)
			&& (bvhDirectShadowHGTable_ != nullptr))
		{
			return true;
		}

		// count materials.
		sl12::u32 totalMaterialCount = 0;
		for (auto&& cmd : tcmds)
		{
			if (cmd->GetType() == sl12::RenderCommandType::Mesh)
			{
				auto mcmd = static_cast<sl12::MeshRenderCommand*>(cmd);
				totalMaterialCount += (sl12::u32)mcmd->GetSubmeshCommands().size();
			}
		}

		// initialize descriptor manager.
		bvhDescMan_ = std::make_unique<sl12::RaytracingDescriptorManager>();
		if (!bvhDescMan_->Initialize(&device_,
			2,		// Render Count
			1,		// AS Count
			4,		// Global CBV Count
			8,		// Global SRV Count
			4,		// Global UAV Count
			4,		// Global Sampler Count
			totalMaterialCount))
		{
			return false;
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
		auto view_desc_size = bvhDescMan_->GetViewDescSize();
		auto sampler_desc_size = bvhDescMan_->GetSamplerDescSize();
		auto local_handle_start = bvhDescMan_->IncrementLocalHandleStart();
		auto FillMeshTable = [&](sl12::MeshRenderCommand* cmd)
		{
			auto pMeshItem = cmd->GetParentMesh()->GetParentResource();
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
		for (auto&& cmd : tcmds)
		{
			if (cmd->GetType() == sl12::RenderCommandType::Mesh)
			{
				FillMeshTable(static_cast<sl12::MeshRenderCommand*>(cmd));
			}
		}

		// create shader table.
		auto Align = [](UINT size, UINT align)
		{
			return ((size + align - 1) / align) * align;
		};
		UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
		UINT shaderRecordSize = Align(descHandleOffset + sizeof(LocalTable), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		bvhShaderRecordSize_ = shaderRecordSize;

		auto GenShaderTable = [&](
			void* const * shaderIds,
			int tableCountPerMaterial,
			std::unique_ptr<sl12::Buffer>& buffer,
			int materialCount)
		{
			buffer = std::make_unique<sl12::Buffer>();

			materialCount = (materialCount < 0) ? (int)material_table.size() : materialCount;
			if (!buffer->Initialize(&device_, shaderRecordSize * tableCountPerMaterial * materialCount, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer->Map(nullptr));
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
			buffer->Unmap();

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
			if (!GenShaderTable(&rgs_identifier, 1, bvhDirectShadowRGSTable_, 1))
			{
				return false;
			}
			if (!GenShaderTable(&ms_identifier, 1, bvhDirectShadowMSTable_, 1))
			{
				return false;
			}
			if (!GenShaderTable(hg_table.data(), kRTMaterialTableCount, bvhDirectShadowHGTable_, -1))
			{
				return false;
			}
		}

		return true;
	}

	void DestroyRaytracing()
	{
		rtGlobalRootSig_.Destroy();
		rtLocalRootSig_.Destroy();
		rtOcclusionCollection_.Destroy();
		rtMaterialCollection_.Destroy();
		rtDirectShadowPSO_.Destroy();

		bvhDirectShadowRGSTable_.reset();
		bvhDirectShadowMSTable_.reset();
		bvhDirectShadowHGTable_.reset();
		bvhDescMan_.reset();
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

	bool CreateNisCoeffTex(sl12::Device* pDev, sl12::CommandList* pCmdList)
	{
		sl12::TextureDesc desc;
		desc.dimension = sl12::TextureDimension::Texture2D;
		desc.width = kFilterSize / 4;
		desc.height = kPhaseCount;
		desc.mipLevels = 1;
		desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		desc.sampleCount = 1;
		desc.clearColor[4] = { 0.0f };
		desc.clearDepth = 1.0f;
		desc.clearStencil = 0;
		desc.isRenderTarget = false;
		desc.isDepthBuffer = false;
		desc.isUav = false;
		desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;

		// NOTE: texture RowPitch is 256. but coef_scale's and coef_usm's RowPitch is 32.
		std::vector<sl12::u8> copyCoefScale, copyCoefUsm;
		copyCoefScale.resize(256 * kPhaseCount);
		copyCoefUsm.resize(256 * kPhaseCount);
		for (size_t y = 0; y < kPhaseCount; y++)
		{
			sl12::u8* ps = copyCoefScale.data() + (256 * y);
			sl12::u8* pu = copyCoefUsm.data() + (256 * y);
			for (size_t x = 0; x < kFilterSize; x++)
			{
				memcpy(ps, &coef_scale[y][x], sizeof(coef_scale[y][x]));
				memcpy(pu, &coef_usm[y][x], sizeof(coef_usm[y][x]));
				ps += sizeof(coef_scale[y][x]);
				pu += sizeof(coef_usm[y][x]);
			}
		}

		if (!nisCoeffScaler_.InitializeFromImageBin(pDev, pCmdList, desc, copyCoefScale.data()))
		{
			return false;
		}
		if (!nisCoeffScalerSRV_.Initialize(pDev, &nisCoeffScaler_))
		{
			return false;
		}

		if (!nisCoeffUsm_.InitializeFromImageBin(pDev, pCmdList, desc, copyCoefUsm.data()))
		{
			return false;
		}
		if (!nisCoeffUsmSRV_.Initialize(pDev, &nisCoeffUsm_))
		{
			return false;
		}

		return true;
	}

private:
	struct RaytracingResult
	{
		sl12::Device*				dev = nullptr;
		sl12::Texture*				tex = nullptr;
		sl12::TextureView*			srv = nullptr;
		sl12::RenderTargetView*		rtv = nullptr;
		sl12::UnorderedAccessView*	uav = nullptr;

		~RaytracingResult()
		{
			Destroy();
		}

		bool Initialize(sl12::Device* pDevice, const sl12::TextureDesc& desc)
		{
			dev = pDevice;

			tex = new sl12::Texture();
			srv = new sl12::TextureView();
			rtv = new sl12::RenderTargetView();
			uav = new sl12::UnorderedAccessView();

			if (!tex->Initialize(pDevice, desc))
			{
				return false;
			}
			if (!srv->Initialize(pDevice, tex))
			{
				return false;
			}
			if (!rtv->Initialize(pDevice, tex))
			{
				return false;
			}
			if (!uav->Initialize(pDevice, tex))
			{
				return false;
			}

			return true;
		}

		void Destroy()
		{
			if (dev)
			{
				if (uav) dev->KillObject(uav);
				if (rtv) dev->KillObject(rtv);
				if (srv) dev->KillObject(srv);
				if (tex) dev->KillObject(tex);
			}
			uav = nullptr;
			rtv = nullptr;
			srv = nullptr;
			tex = nullptr;
		}
	};	// struct RaytracingResult

	struct RenderTargetSet
	{
		sl12::Device*				dev = nullptr;
		sl12::Texture*				tex = nullptr;
		sl12::TextureView*			srv = nullptr;
		sl12::RenderTargetView*		rtv = nullptr;
		sl12::UnorderedAccessView*	uav = nullptr;

		~RenderTargetSet()
		{
			Destroy();
		}

		bool Initialize(sl12::Device* pDev, int width, int height, DXGI_FORMAT format, bool enableUAV)
		{
			dev = pDev;

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
			desc.isUav = enableUAV;
			desc.format = format;

			tex = new sl12::Texture();
			srv = new sl12::TextureView();
			rtv = new sl12::RenderTargetView();

			if (!tex->Initialize(pDev, desc))
			{
				return false;
			}

			if (!srv->Initialize(pDev, tex))
			{
				return false;
			}

			if (!rtv->Initialize(pDev, tex))
			{
				return false;
			}

			if (enableUAV)
			{
				uav = new sl12::UnorderedAccessView();
				if (!uav->Initialize(pDev, tex))
				{
					return false;
				}
			}

			return true;
		}

		void Destroy()
		{
			if (dev)
			{
				if (uav) dev->KillObject(uav);
				if (rtv) dev->KillObject(rtv);
				if (srv) dev->KillObject(srv);
				if (tex) dev->KillObject(tex);
			}
			uav = nullptr;
			rtv = nullptr;
			srv = nullptr;
			tex = nullptr;
		}
	};

	struct GBuffers
	{
		enum GBufferType
		{
			kWorldQuat,
			kBaseColor,
			kMetalRough,
			kVelocity,

			kMax
		};

		sl12::Device*				dev = nullptr;

		RenderTargetSet				rts[kMax];

		sl12::Texture*				depthTex = nullptr;
		sl12::TextureView*			depthSRV = nullptr;
		sl12::DepthStencilView*		depthDSV = nullptr;

		~GBuffers()
		{
			Destroy();
		}

		bool Initialize(sl12::Device* pDev, int width, int height)
		{
			dev = pDev;

			{
				const DXGI_FORMAT kFormats[] = {
					DXGI_FORMAT_R10G10B10A2_UNORM,
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
					DXGI_FORMAT_R8G8B8A8_UNORM,
					DXGI_FORMAT_R16G16_FLOAT,
				};

				for (int i = 0; i < kMax; i++)
				{
					if (!rts[i].Initialize(pDev, width, height, kFormats[i], false))
					{
						return false;
					}
				}
			}

			// 深度バッファを生成
			{
				depthTex = new sl12::Texture();
				depthSRV = new sl12::TextureView();
				depthDSV = new sl12::DepthStencilView();

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
				if (!depthTex->Initialize(pDev, desc))
				{
					return false;
				}

				if (!depthSRV->Initialize(pDev, depthTex))
				{
					return false;
				}

				if (!depthDSV->Initialize(pDev, depthTex))
				{
					return false;
				}
			}

			return true;
		}

		void Destroy()
		{
			if (dev)
			{
				for (int i = 0; i < kMax; i++)
				{
					rts[i].Destroy();
				}
				if (depthDSV) dev->KillObject(depthDSV);
				if (depthSRV) dev->KillObject(depthSRV);
				if (depthTex) dev->KillObject(depthTex);
			}
			depthDSV = nullptr;
			depthSRV = nullptr;
			depthTex = nullptr;
		}
	};	// struct GBuffers

	struct HiZBuffers
	{
		sl12::Device*				dev = nullptr;
		sl12::Texture*				tex = nullptr;
		sl12::TextureView*			srv = nullptr;
		sl12::TextureView*			subSrvs[HIZ_MIP_LEVEL] = {};
		sl12::RenderTargetView*		subRtvs[HIZ_MIP_LEVEL] = {};

		~HiZBuffers()
		{
			Destroy();
		}

		bool Initialize(sl12::Device* pDev, int width, int height)
		{
			dev = pDev;

			tex = new sl12::Texture();
			srv = new sl12::TextureView();

			sl12::TextureDesc desc;
			desc.width = width / 2;
			desc.height = height / 2;
			desc.format = DXGI_FORMAT_R16G16_FLOAT;
			desc.mipLevels = HIZ_MIP_LEVEL;
			desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.isRenderTarget = true;
			desc.isUav = false;

			if (!tex->Initialize(pDev, desc))
			{
				return false;
			}

			if (!srv->Initialize(pDev, tex))
			{
				return false;
			}

			for (int i = 0; i < HIZ_MIP_LEVEL; i++)
			{
				subSrvs[i] = new sl12::TextureView();
				subRtvs[i] = new sl12::RenderTargetView();

				if (!subSrvs[i]->Initialize(pDev, tex, i, 1))
				{
					return false;
				}
				if (!subRtvs[i]->Initialize(pDev, tex, i))
				{
					return false;
				}
			}

			return true;
		}

		void Destroy()
		{
			if (dev)
			{
				for (auto&& v : subSrvs)
					if (v) dev->KillObject(v);
				for (auto&& v : subRtvs)
					if (v) dev->KillObject(v);
				if (srv) dev->KillObject(srv);
				if (tex) dev->KillObject(tex);
			}

			tex = nullptr;
			srv = nullptr;
			memset(subSrvs, 0, sizeof(subSrvs));
			memset(subRtvs, 0, sizeof(subRtvs));
		}
	};

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
	sl12::RootSignature					rtGlobalRootSig_, rtLocalRootSig_;
	sl12::DescriptorSet					rtGlobalDescSet_;
	sl12::DxrPipelineState				rtOcclusionCollection_;
	sl12::DxrPipelineState				rtMaterialCollection_;
	sl12::DxrPipelineState				rtDirectShadowPSO_;

	sl12::Sampler			imageSampler_;
	sl12::Sampler			anisoSamplers_[3];
	sl12::Sampler			clampPointSampler_;
	sl12::Sampler			clampLinearSampler_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer				lightCBs_[kBufferCount];
	sl12::ConstantBufferView	lightCBVs_[kBufferCount];

	sl12::Buffer				giCBs_[kBufferCount];
	sl12::ConstantBufferView	giCBVs_[kBufferCount];

	sl12::Buffer				materialCBs_[kBufferCount];
	sl12::ConstantBufferView	materialCBVs_[kBufferCount];

	sl12::Buffer				DrawCountBuffer_;
	sl12::UnorderedAccessView	DrawCountUAV_;
	sl12::Buffer				DrawCountReadbacks_[kBufferCount];

	// multi draw indirect関連
	sl12::Buffer					frustumCBs_[kBufferCount];
	sl12::ConstantBufferView		frustumCBVs_[kBufferCount];

	GBuffers					gbuffers_;
	RenderTargetSet				accumRTs_[kBufferCount];
	RenderTargetSet				accumTemp_;
	HiZBuffers					HiZ_;
	RenderTargetSet				ldrTarget_;
	RenderTargetSet				fsrTargets_[2];

	sl12::Texture				nisCoeffScaler_, nisCoeffUsm_;
	sl12::TextureView			nisCoeffScalerSRV_, nisCoeffUsmSRV_;

	sl12::Buffer				pointLightsPosBuffer_;
	sl12::BufferView			pointLightsPosSRV_;
	sl12::Buffer				pointLightsColorBuffer_;
	sl12::BufferView			pointLightsColorSRV_;
	sl12::Buffer				clusterInfoBuffer_;
	sl12::BufferView			clusterInfoSRV_;
	sl12::UnorderedAccessView	clusterInfoUAV_;

	ID3D12CommandSignature*		commandSig_ = nullptr;

	sl12::RootSignature			PrePassRS_, GBufferRS_, ToLdrRS_, ReduceDepth1stRS_, ReduceDepth2ndRS_, TargetCopyRS_;
	sl12::RootSignature			ClusterCullRS_, ResetCullDataRS_, Cull1stPhaseRS_, Cull2ndPhaseRS_, LightingRS_, TaaRS_, TaaFirstRS_, FsrEasuRS_, FsrRcasRS_, NisScalerRS_;
	sl12::GraphicsPipelineState	PrePassPSO_, GBufferPSO_, ToLdrPSO_, ReduceDepth1stPSO_, ReduceDepth2ndPSO_, TargetCopyPSO_;
	sl12::ComputePipelineState	ClusterCullPSO_, ResetCullDataPSO_, Cull1stPhasePSO_, Cull2ndPhasePSO_, LightingPSO_, TaaPSO_, TaaFirstPSO_, FsrEasuPSO_, FsrRcasPSO_, NisScalerPSO_;

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

	int						renderWidth_ = kScreenWidth, renderHeight_ = kScreenHeight;
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
	int						currRenderRes_ = 0;
	int						currUpscaler_ = 0;
	float					upscalerSharpness_ = 0.5f;
	bool					useTAA_ = true;
	bool					taaFirstRender_ = true;
	DirectX::XMFLOAT4X4		mtxFrustumViewProj_;
	DirectX::XMFLOAT4		frustumCamPos_;

	int		frameIndex_ = 0;

	sl12::ResourceLoader	resLoader_;
	sl12::ResourceHandle	hSponzaRes_;
	sl12::ResourceHandle	hSuzanneRes_;
	sl12::ResourceHandle	hBlueNoiseRes_;

	sl12::ConstantBufferCache	cbvCache_;
	sl12::ShaderManager			shaderManager_;
	sl12::ShaderHandle			hShaders_[SHADER_MAX];
	std::unique_ptr<sl12::SceneRoot>	sceneRoot_;
	std::shared_ptr<sl12::SceneMesh>	sponzaMesh_, suzanneMesh_;

	// TEST: BVH Manager
	std::unique_ptr<sl12::BvhManager>					bvhManager_;
	sl12::u32											bvhShaderRecordSize_;
	std::unique_ptr<sl12::RaytracingDescriptorManager>	bvhDescMan_;
	std::unique_ptr<sl12::Buffer>						bvhDirectShadowRGSTable_;
	std::unique_ptr<sl12::Buffer>						bvhDirectShadowMSTable_;
	std::unique_ptr<sl12::Buffer>						bvhDirectShadowHGTable_;

	int						sceneState_ = 0;		// 0:loading scene, 1:main scene
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
