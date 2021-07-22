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

#include "CompiledShaders/hybrid.lib.hlsl.h"
#include "CompiledShaders/zpre.vv.hlsl.h"
#include "CompiledShaders/zpre.p.hlsl.h"
#include "CompiledShaders/lighting.vv.hlsl.h"
#include "CompiledShaders/lighting.p.hlsl.h"
#include "CompiledShaders/fullscreen.vv.hlsl.h"
#include "CompiledShaders/temporal.p.hlsl.h"
#include "CompiledShaders/denoise_x.p.hlsl.h"
#include "CompiledShaders/denoise_y.p.hlsl.h"
#include "CompiledShaders/frustum_cull.c.hlsl.h"
#include "CompiledShaders/count_clear.c.hlsl.h"

#include <windowsx.h>


namespace
{
	static const int	kScreenWidth = 1920;
	static const int	kScreenHeight = 1080;
	static const int	MaxSample = 512;
	static const float	kNearZ = 0.01f;
	static const float	kFarZ = 10000.0f;

	static const float	kSponzaScale = 20.0f;
	static const float	kSuzanneScale = 1.0f;

	static const int	kMaxMaterials = 32;

	static LPCWSTR		kRayGenName = L"RayGenerator";
	static LPCWSTR		kAnyHitName = L"AnyHitProcessor";
	static LPCWSTR		kMissName = L"MissProcessor";
	static LPCWSTR		kHitGroupOpaqueName = L"HitGroupOpaque";
	static LPCWSTR		kHitGroupMaskName = L"HitGroupMask";

	static const DXGI_FORMAT	kReytracingResultFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
}

class SampleApplication
	: public sl12::Application
{
	struct SceneCB
	{
		DirectX::XMFLOAT4X4	mtxWorldToProj;
		DirectX::XMFLOAT4X4	mtxProjToWorld;
		DirectX::XMFLOAT4X4	mtxPrevWorldToProj;
		DirectX::XMFLOAT4X4	mtxPrevProjToWorld;
		DirectX::XMFLOAT4	screenInfo;
		DirectX::XMFLOAT4	camPos;
		DirectX::XMFLOAT4	lightDir;
		DirectX::XMFLOAT4	lightColor;
		float				skyPower;
		float				aoLength;
		uint32_t			aoSampleCount;
		uint32_t			loopCount;
		uint32_t			randomType;
		uint32_t			temporalOn;
		uint32_t			aoOnly;
	};

	struct MeshCB
	{
		DirectX::XMFLOAT4X4	mtxLocalToWorld;
		DirectX::XMFLOAT4X4	mtxPrevLocalToWorld;
	};

	struct FrustumCB
	{
		DirectX::XMFLOAT4	frustumPlane[6];
	};

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
		// コマンドリストの初期化
		auto&& gqueue = device_.GetGraphicsQueue();
		auto&& cqueue = device_.GetComputeQueue();
		if (!zpreCmdLists_.Initialize(&device_, &gqueue))
		{
			return false;
		}
		if (!dxrCmdLists_.Initialize(&device_, &cqueue))
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
		if (!asFence_.Initialize(&device_))
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

		// ルートシグネチャの初期化
		if (!sl12::CreateRaytracingRootSignature(&device_, 1, 1, 4, 2, 0, &rtGlobalRootSig_, &rtLocalRootSig_))
		//if (!sl12::CreateRaytracingRootSignature(&device_, 1, 1, 2, 0, 0, &rtGlobalRootSig_, &rtLocalRootSig_))
		{
			return false;
		}
		if (!zpreRootSig_.Initialize(&device_, &zpreVS_, &zprePS_, nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!lightingRootSig_.Initialize(&device_, &lightingVS_, &lightingPS_, nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!temporalRootSig_.Initialize(&device_, &fullscreenVS_, &temporalPS_, nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!cullRootSig_.Initialize(&device_, &cullCS_))
		{
			return false;
		}
		if (!countClearRootSig_.Initialize(&device_, &countClearCS_))
		{
			return false;
		}

		// パイプラインステートオブジェクトの初期化
		if (!CreatePipelineState())
		{
			return false;
		}
		{
			if (!zpreVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pZpreVS, sizeof(g_pZpreVS)))
			{
				return false;
			}
			if (!zprePS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pZprePS, sizeof(g_pZprePS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &zpreRootSig_;
			desc.pVS = &zpreVS_;
			desc.pPS = &zprePS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = true;
			desc.depthStencil.isDepthWriteEnable = true;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
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

			if (!zprePso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!lightingVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pLightingVS, sizeof(g_pLightingVS)))
			{
				return false;
			}
			if (!lightingPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pLightingPS, sizeof(g_pLightingPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &lightingRootSig_;
			desc.pVS = &lightingVS_;
			desc.pPS = &lightingPS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = true;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_EQUAL;

			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			desc.inputLayout.numElements = ARRAYSIZE(input_elems);
			desc.inputLayout.pElements = input_elems;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.multisampleCount = 1;

			if (!lightingPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!fullscreenVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pFullscreenVS, sizeof(g_pFullscreenVS)))
			{
				return false;
			}
			if (!temporalPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pTemporalPS, sizeof(g_pTemporalPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &temporalRootSig_;
			desc.pVS = &fullscreenVS_;
			desc.pPS = &temporalPS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_ONE;
			desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_INV_SRC_ALPHA;
			desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ZERO;
			desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ONE;
			desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_EQUAL;

			desc.inputLayout.numElements = 0;
			desc.inputLayout.pElements = nullptr;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = kReytracingResultFormat;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!temporalPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!denoiseXPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pDenoiseXPS, sizeof(g_pDenoiseXPS)))
			{
				return false;
			}
			if (!denoiseYPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pDenoiseYPS, sizeof(g_pDenoiseYPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &temporalRootSig_;
			desc.pVS = &fullscreenVS_;
			desc.pPS = &denoiseXPS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_EQUAL;

			desc.inputLayout.numElements = 0;
			desc.inputLayout.pElements = nullptr;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = kReytracingResultFormat;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!denoiseXPso_.Initialize(&device_, desc))
			{
				return false;
			}

			desc.pPS = &denoiseYPS_;

			if (!denoiseYPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!cullCS_.Initialize(&device_, sl12::ShaderType::Compute, g_pFrustumCullCS, sizeof(g_pFrustumCullCS)))
			{
				return false;
			}

			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &cullRootSig_;
			desc.pCS = &cullCS_;

			if (!cullPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!countClearCS_.Initialize(&device_, sl12::ShaderType::Compute, g_pCountClearCS, sizeof(g_pCountClearCS)))
			{
				return false;
			}

			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &countClearRootSig_;
			desc.pCS = &countClearCS_;

			if (!countClearPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// 出力先のテクスチャを生成
		{
			sl12::TextureDesc desc;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.mipLevels = 1;
			desc.format = kReytracingResultFormat;
			desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			desc.sampleCount = 1;
			desc.clearColor[0] = desc.clearColor[1] = desc.clearColor[2] = desc.clearColor[3] = 0.0f;
			desc.clearDepth = 1.0f;
			desc.clearStencil = 0;
			desc.isRenderTarget = true;
			desc.isDepthBuffer = false;
			desc.isUav = true;
			if (!resultTexture_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!resultTextureSRV_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}

			if (!resultTextureRTV_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}

			if (!resultTextureUAV_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}

			desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			for (int i = 0; i < ARRAYSIZE(rtResults_); i++)
			{
				if (!rtResults_[i].Initialize(&device_, desc))
				{
					return false;
				}
			}
			if (!rtWork_.Initialize(&device_, desc))
			{
				return false;
			}
			if (!denoiseWork_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// 乱数用のバッファを生成
		{
			if (!randomBuffer_.Initialize(&device_, sizeof(float) * 65536, sizeof(float), sl12::BufferUsage::ShaderResource, true, false))
			{
				return false;
			}
			if (!randomBufferSRV_.Initialize(&device_, &randomBuffer_, 0, 0, sizeof(float)))
			{
				return false;
			}
			if (!seedBuffer_.Initialize(&device_, sizeof(uint32_t), 1, sl12::BufferUsage::ShaderResource, false, true))
			{
				return false;
			}
			if (!seedBufferUAV_.Initialize(&device_, &seedBuffer_, 0, 0, 0, 0))
			{
				return false;
			}

			auto p = reinterpret_cast<float*>(randomBuffer_.Map(nullptr));
			std::random_device rd;
			std::mt19937 mt(rd());
			std::uniform_real_distribution<float> randGen01(0.0f, 1.0f);
			for (int i = 0; i < 65536; i++)
			{
				p[i] = randGen01(mt);
			}
			randomBuffer_.Unmap();
		}

		// タイムスタンプクエリ
		for (int i = 0; i < ARRAYSIZE(gpuTimestamp_); ++i)
		{
			if (!gpuTimestamp_[i].Initialize(&device_, 10))
			{
				return false;
			}
		}
		if (!bottomAsTimestamp_.Initialize(&device_, 2))
		{
			return false;
		}
		for (int i = 0; i < ARRAYSIZE(topAsTimestamp_); ++i)
		{
			if (!topAsTimestamp_[i].Initialize(&device_, 2))
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

		// ブルーノイズ読み込み
		{
			sl12::File texFile("data/blue_noise.tga");

			if (!blueNoiseTex_.InitializeFromTGA(&device_, &utilCmdList_, texFile.GetData(), texFile.GetSize(), 1, false))
			{
				return false;
			}
			if (!blueNoiseSRV_.Initialize(&device_, &blueNoiseTex_))
			{
				return false;
			}
		}

		// ジオメトリを生成する
		if (!CreateGeometry())
		{
			return false;
		}

		utilCmdList_.Close();
		utilCmdList_.Execute();
		device_.WaitDrawDone();

		// multi draw indirectに必要なオブジェクトを生成する
		if (!CreateIndirectDrawParams())
		{
			return false;
		}

		// Raytracing用DescriptorHeapの初期化
		if (!rtDescMan_.Initialize(&device_, 1, 1, 1, 4, 2, 0, std::min<int>(glbMesh_.GetSubmeshCount(), kMaxMaterials)))
		{
			return false;
		}

		// ASを生成する
		if (!CreateAccelerationStructure())
		{
			return false;
		}

		// 定数バッファを生成する
		if (!CreateSceneCB())
		{
			return false;
		}
		if (!CreateMeshCB())
		{
			return false;
		}

		// シェーダテーブルを生成する
		if (!CreateShaderTable())
		{
			return false;
		}

		return true;
	}

	bool Execute() override
	{
		device_.WaitPresent();
		device_.SyncKillObjects();

		const int kSwapchainBufferOffset = 1;
		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto prevFrameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 2) % sl12::Swapchain::kMaxBuffer;
		auto&& zpreCmdList = zpreCmdLists_.Reset();
		auto&& asCmdList = dxrCmdLists_.Reset();
		auto&& litCmdList = litCmdLists_.Reset();
		auto pCmdList = &zpreCmdList;
		auto d3dCmdList = pCmdList->GetCommandList();
		auto dxrCmdList = pCmdList->GetDxrCommandList();
		auto&& curGBuffer = gbuffers_[frameIndex];
		auto&& prevGBuffer = gbuffers_[prevFrameIndex];

		UpdateSceneCB(frameIndex);

		gui_.BeginNewFrame(&litCmdList, kScreenWidth, kScreenHeight, inputData_);

		// カメラ操作
		ControlCamera();
		if (isCameraMove_)
		{
			isCameraMove_ = false;
		}

		// GUI
		{
			static const char* kRandomTypes[] = {
				"None",
				"Table",
				"Blue Noise",
			};

			if (ImGui::SliderFloat("Sky Power", &skyPower_, 0.0f, 10.0f))
			{
			}
			if (ImGui::SliderFloat("Light Intensity", &lightPower_, 0.0f, 10.0f))
			{
			}
			if (ImGui::ColorEdit3("Light Color", lightColor_))
			{
			}
			if (ImGui::SliderFloat("AO Length", &aoLength_, 0.0f, 100.0f))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::SliderInt("AO Sample Count", &aoSampleCount_, 1, 16))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::SliderInt("Denoise Iteration", &denoiseCount_, 0, 4))
			{
				isDenoise_ = denoiseCount_ > 0;
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::Combo("Random Type", &randomType_, kRandomTypes, ARRAYSIZE(kRandomTypes)))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::Checkbox("Temporal", &isTemporalOn_))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			ImGui::Checkbox("AO Only", &isAOOnly_);
			ImGui::Checkbox("Indirect Draw", &isIndirectDraw_);
			ImGui::Checkbox("Freeze Cull", &isFreezeCull_);

			uint64_t freq = device_.GetGraphicsQueue().GetTimestampFrequency();
			uint64_t timestamp[6];

			gpuTimestamp_[frameIndex].GetTimestamp(0, 6, timestamp);
			uint64_t all_time = timestamp[4] - timestamp[0];
			uint64_t ray_time = timestamp[3] - timestamp[2];
			float all_ms = (float)all_time / ((float)freq / 1000.0f);
			float ray_ms = (float)ray_time / ((float)freq / 1000.0f);

			ImGui::Text("All GPU: %f (ms)", all_ms);
			ImGui::Text("RayTracing: %f (ms)", ray_ms);

			bottomAsTimestamp_.GetTimestamp(0, 2, timestamp);
			uint64_t bas_time = timestamp[1] - timestamp[0];
			float bas_ms = (float)bas_time / ((float)freq / 1000.0f);
			ImGui::Text("BottomAS Build: %f (ms)", bas_ms);
		}

		gpuTimestamp_[frameIndex].Reset();
		gpuTimestamp_[frameIndex].Query(pCmdList);

		auto&& swapchain = device_.GetSwapchain();
		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle, color, 0, nullptr);
		}

		if (isClearTarget_)
		{
			isClearTarget_ = false;
			float color[4] = { 0, 0, 0, 0 };
			for (int i = 0; i < ARRAYSIZE(rtResults_); i++)
			{
				pCmdList->TransitionBarrier(&rtResults_[i].tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				d3dCmdList->ClearRenderTargetView(rtResults_[i].rtv.GetDescInfo().cpuHandle, color, 0, nullptr);
				pCmdList->TransitionBarrier(&rtResults_[i].tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
		}
		d3dCmdList->ClearDepthStencilView(curGBuffer.depthDSV.GetDescInfo().cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[0], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[1], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[2], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

		gpuTimestamp_[frameIndex].Query(pCmdList);

		// GPU culling
		if (isIndirectDraw_)
		{
			// Barrier
			pCmdList->TransitionBarrier(&indirectArgumentB_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			pCmdList->TransitionBarrier(&indirectCounterB_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// カウントバッファをクリア
			d3dCmdList->SetPipelineState(countClearPso_.GetPSO());
			cullDescSet_.Reset();
			cullDescSet_.SetCsUav(0, indirectCounterUAVs_[0]->GetDescInfo().cpuHandle);
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&countClearRootSig_, &cullDescSet_);
			d3dCmdList->Dispatch((UINT)meshletCounts_.size(), 1, 1);

			// PSO設定
			d3dCmdList->SetPipelineState(cullPso_.GetPSO());

			// 基本Descriptor設定
			cullDescSet_.Reset();
			cullDescSet_.SetCsCbv(0, frustumCBVs_[frameIndex].GetDescInfo().cpuHandle);

			// サブメッシュのカリング処理実行
			int dispatch_count = 0;
			auto Dispatch = [&](sl12::GlbMesh* pMesh, sl12::ConstantBufferView* pMeshCBV)
			{
				cullDescSet_.SetCsCbv(1, pMeshCBV->GetDescInfo().cpuHandle);

				auto submesh_count = pMesh->GetSubmeshCount();
				for (int i = 0; i < submesh_count; i++, dispatch_count++)
				{
					int dispatch_x = meshletCounts_[dispatch_count];
					auto bv = meshletBVs_[dispatch_count];
					auto arg_uav = indirectArgumentUAVs_[dispatch_count];
					auto cnt_uav = indirectCounterUAVs_[dispatch_count];

					cullDescSet_.SetCsSrv(0, bv->GetDescInfo().cpuHandle);
					cullDescSet_.SetCsUav(0, arg_uav->GetDescInfo().cpuHandle);
					cullDescSet_.SetCsUav(1, cnt_uav->GetDescInfo().cpuHandle);

					pCmdList->SetComputeRootSignatureAndDescriptorSet(&cullRootSig_, &cullDescSet_);

					d3dCmdList->Dispatch(dispatch_x, 1, 1);
				}
			};

			Dispatch(&glbMesh_, &glbMeshCBV_);

			// Barrier
			pCmdList->TransitionBarrier(&indirectArgumentB_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
			pCmdList->TransitionBarrier(&indirectCounterB_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		// Z pre pass
		{
			// レンダーターゲット設定
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

			// PSO設定
			d3dCmdList->SetPipelineState(zprePso_.GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetVsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, linearWrapSampler_.GetDescInfo().cpuHandle);

			int count = 0;
			int offset = 0;
			auto RenderMesh = [&](sl12::GlbMesh* pMesh, sl12::ConstantBufferView* pMeshCBV)
			{
				descSet_.SetVsCbv(1, pMeshCBV->GetDescInfo().cpuHandle);

				auto submesh_count = pMesh->GetSubmeshCount();
				for (int i = 0; i < submesh_count; i++, count++)
				{
					auto&& submesh = pMesh->GetSubmesh(i);
					auto&& material = pMesh->GetMaterial(submesh->GetMaterialIndex());
					auto&& base_color_srv = pMesh->GetTextureView(material->GetTexBaseColorIndex());

					descSet_.SetPsSrv(0, base_color_srv->GetDescInfo().cpuHandle);
					pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&zpreRootSig_, &descSet_);

					const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
						submesh->GetPositionVBV().GetView(),
						submesh->GetNormalVBV().GetView(),
						submesh->GetTexcoordVBV().GetView(),
					};
					d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

					auto&& ibv = submesh->GetIndexIBV().GetView();
					d3dCmdList->IASetIndexBuffer(&ibv);

					if (isIndirectDraw_)
					{
						int command_count = meshletCounts_[count];
						d3dCmdList->ExecuteIndirect(
							commandSig_,											// command signature
							command_count,											// コマンドの最大発行回数
							indirectArgumentB_.GetResourceDep(),					// indirectコマンドの変数バッファ
							sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * offset,			// indirectコマンドの変数バッファの先頭オフセット
							indirectCounterB_.GetResourceDep(),						// 実際の発行回数を収めたカウントバッファ
							sizeof(sl12::u32) * 4 * count);							// カウントバッファの先頭オフセット
						offset += command_count;
					}
					else
					{
						d3dCmdList->DrawIndexedInstanced(submesh->GetIndicesCount(), 1, 0, 0, 0);
					}
				}
			};

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			RenderMesh(&glbMesh_, &glbMeshCBV_);
		}

		// リソースバリア
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[1], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[2], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

		// コマンドリスト変更
		pCmdList = &litCmdList;
		d3dCmdList = pCmdList->GetCommandList();
		dxrCmdList = pCmdList->GetDxrCommandList();

		pCmdList->SetDescriptorHeapDirty();

		// Shadow & AO
		RaytracingResult* pRTResult = &rtWork_;
		RaytracingResult* pTRResult = &rtResults_[frameIndex];
		RaytracingResult* pDNResult = &rtWork_;
		{
			pCmdList->TransitionBarrier(&pRTResult->tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// レイトレのみの時間計測
			gpuTimestamp_[frameIndex].Query(pCmdList);

			// デスクリプタを設定
			rtGlobalDescSet_.Reset();
			rtGlobalDescSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(1, curGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(2, curGBuffer.depthSRV.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(3, randomBufferSRV_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(4, blueNoiseSRV_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(0, pRTResult->uav.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(1, seedBufferUAV_.GetDescInfo().cpuHandle);

			// コピーしつつコマンドリストに積む
			D3D12_GPU_VIRTUAL_ADDRESS as_address[] = {
				topAS_.GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress(),
			};
			pCmdList->SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, &rtDescMan_, as_address, ARRAYSIZE(as_address));

			// レイトレースを実行
			D3D12_DISPATCH_RAYS_DESC desc{};
			desc.HitGroupTable.StartAddress = hitGroupTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.HitGroupTable.SizeInBytes = hitGroupTable_.GetSize();
			desc.HitGroupTable.StrideInBytes = shaderRecordSize_;
			desc.MissShaderTable.StartAddress = missTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.MissShaderTable.SizeInBytes = missTable_.GetSize();
			desc.MissShaderTable.StrideInBytes = shaderRecordSize_;
			desc.RayGenerationShaderRecord.StartAddress = rayGenTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = rayGenTable_.GetSize();
			desc.Width = kScreenWidth;
			desc.Height = kScreenHeight;
			desc.Depth = 1;
			dxrCmdList->SetPipelineState1(stateObject_.GetPSO());
			dxrCmdList->DispatchRays(&desc);

			pCmdList->UAVBarrier(&pRTResult->tex);

			// レイトレはここまで
			gpuTimestamp_[frameIndex].Query(pCmdList);

			pCmdList->TransitionBarrier(&pRTResult->tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			pCmdList->TransitionBarrier(&prevGBuffer.depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

			// Temporal
			{
				pCmdList->TransitionBarrier(&pTRResult->tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

				auto&& rtv = pTRResult->rtv.GetDescInfo().cpuHandle;
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

				// PSO設定
				d3dCmdList->SetPipelineState(temporalPso_.GetPSO());

				// 基本Descriptor設定
				descSet_.Reset();
				descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(0, curGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(1, curGBuffer.gbufferSRV[1].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(2, curGBuffer.gbufferSRV[2].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(3, pRTResult->srv.GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(4, prevGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(5, prevGBuffer.gbufferSRV[1].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(6, rtResults_[prevFrameIndex].srv.GetDescInfo().cpuHandle);
				descSet_.SetPsSampler(0, linearClampSampler_.GetDescInfo().cpuHandle);
				descSet_.SetPsSampler(1, pointClampSampler_.GetDescInfo().cpuHandle);
				pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&temporalRootSig_, &descSet_);

				// DrawCall
				d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d3dCmdList->IASetVertexBuffers(0, 0, nullptr);
				d3dCmdList->IASetIndexBuffer(nullptr);
				d3dCmdList->DrawInstanced(3, 1, 0, 0);

				pCmdList->TransitionBarrier(&pTRResult->tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}

			// Denoise
			for (int cnt = 0; cnt < std::max(1, denoiseCount_); cnt++)
			{
				// X
				pCmdList->TransitionBarrier(&denoiseWork_.tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				auto rtv = denoiseWork_.rtv.GetDescInfo().cpuHandle;
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

				// PSO設定
				d3dCmdList->SetPipelineState(denoiseXPso_.GetPSO());

				// 基本Descriptor設定
				descSet_.Reset();
				descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(0, curGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(1, curGBuffer.gbufferSRV[1].GetDescInfo().cpuHandle);
				descSet_.SetPsSrv(2, pTRResult->srv.GetDescInfo().cpuHandle);
				pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&temporalRootSig_, &descSet_);

				// DrawCall
				d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d3dCmdList->IASetVertexBuffers(0, 0, nullptr);
				d3dCmdList->IASetIndexBuffer(nullptr);
				d3dCmdList->DrawInstanced(3, 1, 0, 0);

				pCmdList->TransitionBarrier(&denoiseWork_.tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

				// Y
				pCmdList->TransitionBarrier(&pDNResult->tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				rtv = pDNResult->rtv.GetDescInfo().cpuHandle;
				d3dCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

				// PSO設定
				d3dCmdList->SetPipelineState(denoiseYPso_.GetPSO());

				// 基本Descriptor設定
				descSet_.SetPsSrv(2, denoiseWork_.srv.GetDescInfo().cpuHandle);
				pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&temporalRootSig_, &descSet_);

				// DrawCall
				d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d3dCmdList->IASetVertexBuffers(0, 0, nullptr);
				d3dCmdList->IASetIndexBuffer(nullptr);
				d3dCmdList->DrawInstanced(3, 1, 0, 0);

				pCmdList->TransitionBarrier(&pDNResult->tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

				pTRResult = pDNResult;
			}

			pCmdList->TransitionBarrier(&prevGBuffer.depthTex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		// lighting pass
		{
			// レンダーターゲット設定
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

			// PSO設定
			d3dCmdList->SetPipelineState(lightingPso_.GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetVsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			if (isDenoise_)
				descSet_.SetPsSrv(1, rtWork_.srv.GetDescInfo().cpuHandle);
			else
				descSet_.SetPsSrv(1, rtResults_[frameIndex].srv.GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, linearWrapSampler_.GetDescInfo().cpuHandle);

			int count = 0;
			int offset = 0;
			auto RenderMesh = [&](sl12::GlbMesh* pMesh, sl12::ConstantBufferView* pMeshCBV)
			{
				descSet_.SetVsCbv(1, pMeshCBV->GetDescInfo().cpuHandle);

				auto submesh_count = pMesh->GetSubmeshCount();
				for (int i = 0; i < submesh_count; i++, count++)
				{
					auto&& submesh = pMesh->GetSubmesh(i);
					auto&& material = pMesh->GetMaterial(submesh->GetMaterialIndex());
					auto&& base_color_srv = pMesh->GetTextureView(material->GetTexBaseColorIndex());

					descSet_.SetPsSrv(0, base_color_srv->GetDescInfo().cpuHandle);
					pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&lightingRootSig_, &descSet_);

					const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
						submesh->GetPositionVBV().GetView(),
						submesh->GetNormalVBV().GetView(),
						submesh->GetTexcoordVBV().GetView(),
					};
					d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

					auto&& ibv = submesh->GetIndexIBV().GetView();
					d3dCmdList->IASetIndexBuffer(&ibv);

					if (isIndirectDraw_)
					{
						int command_count = meshletCounts_[count];
						d3dCmdList->ExecuteIndirect(
							commandSig_,											// command signature
							command_count,											// コマンドの最大発行回数
							indirectArgumentB_.GetResourceDep(),					// indirectコマンドの変数バッファ
							sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * offset,			// indirectコマンドの変数バッファの先頭オフセット
							indirectCounterB_.GetResourceDep(),						// 実際の発行回数を収めたカウントバッファ
							sizeof(sl12::u32) * 4 * count);							// カウントバッファの先頭オフセット
						offset += command_count;
					}
					else
					{
						d3dCmdList->DrawIndexedInstanced(submesh->GetIndicesCount(), 1, 0, 0, 0);
					}
				}
			};

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			RenderMesh(&glbMesh_, &glbMeshCBV_);
		}

		ImGui::Render();

		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		gpuTimestamp_[frameIndex].Query(pCmdList);
		gpuTimestamp_[frameIndex].Resolve(pCmdList);

		// コマンド終了と描画待ち
		zpreCmdLists_.Close();
		dxrCmdLists_.Close();
		litCmdLists_.Close();
		device_.WaitDrawDone();

		// 次のフレームへ
		device_.Present(0);

		// コマンド実行
		zpreCmdLists_.Execute();
		dxrCmdLists_.Execute();
		asFence_.Signal(dxrCmdLists_.GetParentQueue());
		asFence_.WaitSignal(litCmdLists_.GetParentQueue());
		litCmdLists_.Execute();

		return true;
	}

	void Finalize() override
	{
		// 描画待ち
		device_.WaitDrawDone();
		device_.Present(0);

		rayGenTable_.Destroy();
		missTable_.Destroy();
		hitGroupTable_.Destroy();

		glbMeshCBV_.Destroy();
		glbMeshCB_.Destroy();

		for (auto&& v : sceneCBVs_) v.Destroy();
		for (auto&& v : sceneCBs_) v.Destroy();

		for (auto&& v : gpuTimestamp_) v.Destroy();

		gui_.Destroy();

		topAS_.Destroy();
		glbBottomAS_.Destroy();

		DestroyIndirectDrawParams();

		glbMesh_.Destroy();

		instanceSBV_.Destroy();
		instanceSB_.Destroy();
		spheresAABB_.Destroy();

		randomBufferSRV_.Destroy();
		randomBuffer_.Destroy();
		seedBufferUAV_.Destroy();
		seedBuffer_.Destroy();

		for (auto&& v : gbuffers_) v.Destroy();

		denoiseWork_.Destroy();
		rtWork_.Destroy();
		for (auto&& v : rtResults_) v.Destroy();
		resultTextureUAV_.Destroy();
		resultTextureRTV_.Destroy();
		resultTextureSRV_.Destroy();
		resultTexture_.Destroy();

		countClearPso_.Destroy();
		countClearCS_.Destroy();
		cullPso_.Destroy();
		cullCS_.Destroy();
		denoiseXPso_.Destroy();
		denoiseYPso_.Destroy();
		denoiseXPS_.Destroy();
		denoiseYPS_.Destroy();
		temporalPso_.Destroy();
		fullscreenVS_.Destroy();
		temporalPS_.Destroy();
		lightingPso_.Destroy();
		lightingVS_.Destroy();
		lightingPS_.Destroy();
		zprePso_.Destroy();
		zpreVS_.Destroy();
		zprePS_.Destroy();
		stateObject_.Destroy();

		rtDescMan_.Destroy();
		countClearRootSig_.Destroy();
		cullRootSig_.Destroy();
		temporalRootSig_.Destroy();
		lightingRootSig_.Destroy();
		zpreRootSig_.Destroy();
		rtLocalRootSig_.Destroy();
		rtGlobalRootSig_.Destroy();

		asFence_.Destroy();
		utilCmdList_.Destroy();
		litCmdLists_.Destroy();
		dxrCmdLists_.Destroy();
		zpreCmdLists_.Destroy();
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
	bool CreatePipelineState()
	{
		// DXR用のパイプラインステートを生成します.
		// Graphics用、Compute用のパイプラインステートは生成時に固定サイズの記述子を用意します.
		// これに対して、DXR用のパイプラインステートは可変個のサブオブジェクトを必要とします.
		// また、サブオブジェクトの中には他のサブオブジェクトへのポインタを必要とするものもあるため、サブオブジェクト配列の生成には注意が必要です.

		sl12::DxrPipelineStateDesc dxrDesc;

		// DXILライブラリサブオブジェクト
		// 1つのシェーダライブラリと、そこに登録されているシェーダのエントリーポイントをエクスポートするためのサブオブジェクトです.
		D3D12_EXPORT_DESC libExport[] = {
			{ kRayGenName,		nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kAnyHitName,		nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kMissName,		nullptr, D3D12_EXPORT_FLAG_NONE },
		};
		dxrDesc.AddDxilLibrary(g_pHybridLib, sizeof(g_pHybridLib), libExport, ARRAYSIZE(libExport));

		// ヒットグループサブオブジェクト
		// Intersection, AnyHit, ClosestHitの組み合わせを定義し、ヒットグループ名でまとめるサブオブジェクトです.
		// マテリアルごとや用途ごと(マテリアル、シャドウなど)にサブオブジェクトを用意します.
		dxrDesc.AddHitGroup(kHitGroupOpaqueName, true, nullptr, nullptr, nullptr);
		dxrDesc.AddHitGroup(kHitGroupMaskName, true, kAnyHitName, nullptr, nullptr);

		// シェーダコンフィグサブオブジェクト
		// ヒットシェーダ、ミスシェーダの引数となるPayload, IntersectionAttributesの最大サイズを設定します.
		dxrDesc.AddShaderConfig(sizeof(float) * 16, sizeof(float) * 2);

		// ローカルルートシグネチャサブオブジェクト
		// シェーダレコードごとに設定されるルートシグネチャを設定します.

		// Exports Assosiation サブオブジェクト
		// シェーダレコードとローカルルートシグネチャのバインドを行うサブオブジェクトです.
		LPCWSTR kExports[] = {
			kRayGenName,
			kMissName,
			kHitGroupOpaqueName,
			kHitGroupMaskName,
		};
		dxrDesc.AddLocalRootSignatureAndExportAssociation(rtLocalRootSig_, kExports, ARRAYSIZE(kExports));

		// グローバルルートシグネチャサブオブジェクト
		// すべてのシェーダテーブルで参照されるグローバルなルートシグネチャを設定します.
		dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);

		// レイトレースコンフィグサブオブジェクト
		// TraceRay()を行うことができる最大深度を指定するサブオブジェクトです.
		dxrDesc.AddRaytracinConfig(1);

		// PSO生成
		if (!stateObject_.Initialize(&device_, dxrDesc))
		{
			return false;
		}

		return true;
	}

	bool CreateGeometry()
	{
		if (!glbMesh_.Initialize(&device_, &utilCmdList_, "data/", "sponza.glb"))
		{
			return false;
		}

		return true;
	}

	bool CreateBottomASFromGlbMesh(sl12::CommandList* pCmdList, sl12::GlbMesh* pMesh, sl12::BottomAccelerationStructure* pBottomAS)
	{
		// Bottom ASの生成準備
		std::vector< sl12::GeometryStructureDesc> geoDescs(pMesh->GetSubmeshCount());
		for (int i = 0; i < pMesh->GetSubmeshCount(); i++)
		{
			auto submesh = pMesh->GetSubmesh(i);
			auto material = glbMesh_.GetMaterial(submesh->GetMaterialIndex());

			auto flags = material->GetBlendMode() == sl12::GlbMaterial::BlendMode::Opaque
				? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE
				: D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			geoDescs[i].InitializeAsTriangle(
				flags,
				&submesh->GetPositionB(),
				&submesh->GetIndexB(),
				nullptr,
				(UINT64)submesh->GetPositionB().GetStride(),
				static_cast<UINT>(submesh->GetPositionB().GetSize() / submesh->GetPositionB().GetStride()),
				DXGI_FORMAT_R32G32B32_FLOAT,
				static_cast<UINT>(submesh->GetIndexB().GetSize() / submesh->GetIndexB().GetStride()),
				DXGI_FORMAT_R32_UINT);
		}

		sl12::StructureInputDesc bottomInput{};
		if (!bottomInput.InitializeAsBottom(&device_, geoDescs.data(), pMesh->GetSubmeshCount(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
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
		if (!pTopAS->Build(pCmdList, topInput, false))
		{
			return false;
		}

		return true;
	}

	bool UpdateTopAS(sl12::CommandList* pCmdList, sl12::TopInstanceDesc* pInstances, int instanceCount, sl12::TopAccelerationStructure* pTopAS)
	{
		sl12::StructureInputDesc topInput{};
		if (!topInput.InitializeAsTop(&device_, instanceCount, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		if (!pTopAS->CreateInstanceBuffer(&device_, pInstances, instanceCount))
		{
			return false;
		}

		// コマンド発行
		if (!pTopAS->Build(pCmdList, topInput, false))
		{
			return false;
		}

		return true;
	}

	bool CreateAccelerationStructure()
	{
		// ASの生成はGPUで行うため、コマンドを積みGPUを動作させる必要があります.
		auto&& cmdList = utilCmdList_;
		cmdList.Reset();

		bottomAsTimestamp_.Reset();
		bottomAsTimestamp_.Query(&cmdList);

		// BottomASの生成
		if (!CreateBottomASFromGlbMesh(&cmdList, &glbMesh_, &glbBottomAS_))
		{
			return false;
		}

		bottomAsTimestamp_.Query(&cmdList);
		bottomAsTimestamp_.Resolve(&cmdList);

		// Top ASの生成準備
		std::array<sl12::TopInstanceDesc, 1> topInstances;
		DirectX::XMFLOAT4X4 mtx0, mtx1;
		DirectX::XMMATRIX scale0 = DirectX::XMMatrixScaling(kSponzaScale, kSponzaScale, kSponzaScale);
		DirectX::XMMATRIX scale1 = DirectX::XMMatrixScaling(kSuzanneScale, kSuzanneScale, kSuzanneScale);
		DirectX::XMStoreFloat4x4(&mtx0, scale0);
		DirectX::XMStoreFloat4x4(&mtx1, scale1);
		topInstances[0].Initialize(mtx0, 0, 0xff, 0, 0, &glbBottomAS_);

		if (!CreateTopAS(&cmdList, topInstances.data(), (int)topInstances.size(), &topAS_))
		{
			return false;
		}

		// コマンド実行と終了待ち
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		glbBottomAS_.DestroyScratchBuffer();
		device_.KillObject(topAS_.TransferInstanceBuffer());

		return true;
	}

	bool CreateSceneCB()
	{
		// レイ生成シェーダで使用する定数バッファを生成する
		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			DirectX::XMLoadFloat4(&camPos_),
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, kNearZ, kFarZ);
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
			else
			{
				auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[i].Map(nullptr));
				DirectX::XMStoreFloat4x4(&cb->mtxWorldToProj, mtxWorldToClip);
				DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
				DirectX::XMStoreFloat4x4(&cb->mtxPrevWorldToProj, mtxWorldToClip);
				DirectX::XMStoreFloat4x4(&cb->mtxPrevProjToWorld, mtxClipToWorld);
				cb->screenInfo.x = kNearZ;
				cb->screenInfo.y = kFarZ;
				cb->camPos = camPos_;
				cb->lightDir = lightDir;
				cb->lightColor = lightColor;
				cb->skyPower = skyPower_;
				cb->loopCount = 0;
				sceneCBs_[i].Unmap();

				if (!sceneCBVs_[i].Initialize(&device_, &sceneCBs_[i]))
				{
					return false;
				}
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

	bool CreateShaderTable()
	{
		// LocalRS用のマテリアルDescriptorを用意する
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpu_handles;
		std::vector<int> hitGroupIndices;
		auto view_desc_size = rtDescMan_.GetViewDescSize();
		auto sampler_desc_size = rtDescMan_.GetSamplerDescSize();
		auto local_handle_start = rtDescMan_.IncrementLocalHandleStart();

		for (int i = 0; i < std::min<int>(glbMesh_.GetSubmeshCount(), kMaxMaterials); i++)
		{
			auto submesh = glbMesh_.GetSubmesh(i);
			auto material = glbMesh_.GetMaterial(submesh->GetMaterialIndex());
			auto texView = glbMesh_.GetTextureView(material->GetTexBaseColorIndex());
			auto texNView = glbMesh_.GetTextureView(material->GetTexNormalIndex());

			if (material->GetBlendMode() == sl12::GlbMaterial::BlendMode::Opaque)
			{
				hitGroupIndices.push_back(0);
			}
			else
			{
				hitGroupIndices.push_back(1);
			}

#if 1
			// CBVはなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);

			// SRVは3つ
			D3D12_CPU_DESCRIPTOR_HANDLE srv[] = {
				submesh->GetIndexBV().GetDescInfo().cpuHandle,
				submesh->GetTexcoordBV().GetDescInfo().cpuHandle,
				texView->GetDescInfo().cpuHandle,
			};
			sl12::u32 srv_cnt = ARRAYSIZE(srv);
			device_.GetDeviceDep()->CopyDescriptors(
				1, &local_handle_start.viewCpuHandle, &srv_cnt,
				srv_cnt, srv, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			gpu_handles.push_back(local_handle_start.viewGpuHandle);
			local_handle_start.viewCpuHandle.ptr += view_desc_size * srv_cnt;
			local_handle_start.viewGpuHandle.ptr += view_desc_size * srv_cnt;

			// UAVはなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);
#else
			// CBV
			gpu_handles.push_back(local_handle_start.viewGpuHandle);

			// SRV
			D3D12_CPU_DESCRIPTOR_HANDLE srv[] = {
				randomBufferSRV_.GetDescInfo().cpuHandle,
				blueNoiseSRV_.GetDescInfo().cpuHandle,
				submesh->GetIndexBV().GetDescInfo().cpuHandle,
				submesh->GetTexcoordBV().GetDescInfo().cpuHandle,
				texView->GetDescInfo().cpuHandle,
			};
			sl12::u32 srv_cnt = ARRAYSIZE(srv);
			device_.GetDeviceDep()->CopyDescriptors(
				1, &local_handle_start.viewCpuHandle, &srv_cnt,
				srv_cnt, srv, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			gpu_handles.push_back(local_handle_start.viewGpuHandle);
			local_handle_start.viewCpuHandle.ptr += view_desc_size * srv_cnt;
			local_handle_start.viewGpuHandle.ptr += view_desc_size * srv_cnt;

			// UAV
			D3D12_CPU_DESCRIPTOR_HANDLE uav[] = {
				rtWork_.uav.GetDescInfo().cpuHandle,
				seedBufferUAV_.GetDescInfo().cpuHandle,
			};
			sl12::u32 uav_cnt = ARRAYSIZE(uav);
			device_.GetDeviceDep()->CopyDescriptors(
				1, &local_handle_start.viewCpuHandle, &uav_cnt,
				uav_cnt, uav, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			gpu_handles.push_back(local_handle_start.viewGpuHandle);
			local_handle_start.viewCpuHandle.ptr += view_desc_size * uav_cnt;
			local_handle_start.viewGpuHandle.ptr += view_desc_size * uav_cnt;
#endif

			// Samplerは1つ
			D3D12_CPU_DESCRIPTOR_HANDLE sampler[] = {
				imageSampler_.GetDescInfo().cpuHandle,
			};
			sl12::u32 sampler_cnt = ARRAYSIZE(sampler);
			device_.GetDeviceDep()->CopyDescriptors(
				1, &local_handle_start.samplerCpuHandle, &sampler_cnt,
				sampler_cnt, sampler, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			gpu_handles.push_back(local_handle_start.samplerGpuHandle);
			local_handle_start.samplerCpuHandle.ptr += sampler_desc_size * sampler_cnt;
			local_handle_start.samplerGpuHandle.ptr += sampler_desc_size * sampler_cnt;
		}

		// レイ生成シェーダ、ミスシェーダ、ヒットグループのIDを取得します.
		// 各シェーダ種別ごとにシェーダテーブルを作成しますが、このサンプルでは各シェーダ種別はそれぞれ1つのシェーダを持つことになります.
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier[2];
		{
			ID3D12StateObjectProperties* prop;
			stateObject_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
			rayGenShaderIdentifier = prop->GetShaderIdentifier(kRayGenName);
			missShaderIdentifier = prop->GetShaderIdentifier(kMissName);
			hitGroupShaderIdentifier[0] = prop->GetShaderIdentifier(kHitGroupOpaqueName);
			hitGroupShaderIdentifier[1] = prop->GetShaderIdentifier(kHitGroupMaskName);
			prop->Release();
		}

		UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		auto Align = [](UINT size, UINT align)
		{
			return ((size + align - 1) / align) * align;
		};

		// シェーダレコードサイズ
		// シェーダレコードはシェーダテーブルの要素1つです.
		// これはシェーダIDとローカルルートシグネチャに設定される変数の組み合わせで構成されています.
		// シェーダレコードのサイズはシェーダテーブル内で同一でなければならないため、同一シェーダテーブル内で最大のレコードサイズを指定すべきです.
		// 本サンプルではすべてのシェーダレコードについてサイズが同一となります.
		UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
		UINT shaderRecordSize = Align(descHandleOffset + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * 4, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		shaderRecordSize_ = shaderRecordSize;

		auto GenShaderTable = [&](void** shaderIds, int shaderIdsCount, sl12::Buffer& buffer, int count = 1, const int* offsets = nullptr)
		{
			if (!buffer.Initialize(&device_, shaderRecordSize * count * shaderIdsCount, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer.Map(nullptr));

			for (int i = 0; i < count; ++i)
			{
				for (int id = 0; id < shaderIdsCount; ++id)
				{
					auto start = p;

					auto offset = offsets == nullptr ? 0 : offsets[i];

					memcpy(p, shaderIds[id + offset], shaderIdentifierSize);
					p += descHandleOffset;

					memcpy(p, gpu_handles.data() + i * 4, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * 4);

					p = start + shaderRecordSize;
				}
			}
			buffer.Unmap();

			return true;
		};

		if (!GenShaderTable(&rayGenShaderIdentifier, 1, rayGenTable_))
		{
			return false;
		}
		if (!GenShaderTable(&missShaderIdentifier, 1, missTable_))
		{
			return false;
		}
		if (!GenShaderTable(hitGroupShaderIdentifier, 1, hitGroupTable_, std::min<int>(glbMesh_.GetSubmeshCount(), kMaxMaterials), hitGroupIndices.data()))
		{
			return false;
		}

		return true;
	}

	void UpdateSceneCB(int frameIndex)
	{
		auto cp = DirectX::XMLoadFloat4(&camPos_);
		auto mtxWorldToView = DirectX::XMLoadFloat4x4(&mtxWorldToView_);
		auto mtxPrevWorldToView = DirectX::XMLoadFloat4x4(&mtxPrevWorldToView_);
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, kNearZ, kFarZ);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);
		auto mtxPrevWorldToClip = mtxPrevWorldToView * mtxViewToClip;
		auto mtxPrevClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxPrevWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 0.1f, -1.0f, 0.1f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { lightColor_[0] * lightPower_, lightColor_[1] * lightPower_, lightColor_[2] * lightPower_, 1.0f };

		auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[frameIndex].Map(nullptr));
		DirectX::XMStoreFloat4x4(&cb->mtxWorldToProj, mtxWorldToClip);
		DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
		DirectX::XMStoreFloat4x4(&cb->mtxPrevWorldToProj, mtxPrevWorldToClip);
		DirectX::XMStoreFloat4x4(&cb->mtxPrevProjToWorld, mtxPrevClipToWorld);
		cb->screenInfo.x = kNearZ;
		cb->screenInfo.y = kFarZ;
		DirectX::XMStoreFloat4(&cb->camPos, cp);
		cb->lightDir = lightDir;
		cb->lightColor = lightColor;
		cb->skyPower = skyPower_;
		cb->aoLength = aoLength_;
		cb->aoSampleCount = static_cast<uint32_t>(aoSampleCount_);
		cb->loopCount = loopCount_++;
		cb->randomType = randomType_;
		cb->temporalOn = isTemporalOn_ ? 1 : 0;
		cb->aoOnly = isAOOnly_ ? 1 : 0;
		sceneCBs_[frameIndex].Unmap();

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
		if (GetKeyState(VK_UP) < 0)
		{
			isCameraMove_ = true;
			camRotX_ = kRotAngle;
		}
		else if (GetKeyState(VK_DOWN) < 0)
		{
			isCameraMove_ = true;
			camRotX_ = -kRotAngle;
		}
		if (GetKeyState(VK_LEFT) < 0)
		{
			isCameraMove_ = true;
			camRotY_ = -kRotAngle;
		}
		else if (GetKeyState(VK_RIGHT) < 0)
		{
			isCameraMove_ = true;
			camRotY_ = kRotAngle;
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
			camMoveLeft_ = kMoveSpeed;
		}
		else if (GetKeyState('D') < 0)
		{
			isCameraMove_ = true;
			camMoveLeft_ = -kMoveSpeed;
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
		auto mtx_world_to_view = DirectX::XMMatrixLookAtLH(
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

		// Meshletを作成する
		// NOTE: Mesh ShaderのMeshletを意識した構造体バッファを作成します.
		//       Meshletはサブメッシュを[kBaseTriCount]tri前後で分割し、分割ごとにBoundingBoxを生成します.
		//       構造体バッファには各Meshletのインデックスバッファ先頭オフセット、描画インデックス数、AABBの最大・最小値を持つようにします.
		//       Compute Shaderにてこれらの情報をチェックし、カリングを行ってmulti draw indirectのArgumentBufferに登録します.
		struct Meshlet
		{
			DirectX::XMFLOAT3	aabbMin;
			sl12::u32			indexOffset;
			DirectX::XMFLOAT3	aabbMax;
			sl12::u32			indexCount;
		};
		std::vector<Meshlet> tmp_meshlets;
		meshletCounts_.clear();
		auto CreateMeshlets = [&](sl12::GlbMesh& mesh)
		{
			for (int i = 0; i < mesh.GetSubmeshCount(); i++)
			{
				auto&& submesh = mesh.GetSubmesh(i);
				auto pos = submesh->GetPositionSourceData();
				auto idx = submesh->GetIndexSourceData();
				auto tri_count = submesh->GetIndicesCount() / 3;
				int offset = 0;
				int meshlet_count = 0;
				while (tri_count > 0)
				{
					static const int kBaseTriCount = 256;
					auto crr_count = std::min(kBaseTriCount, tri_count);
					if (tri_count - crr_count < kBaseTriCount / 2)
					{
						crr_count = tri_count;
					}

					Meshlet meshlet;
					meshlet.indexOffset = offset;
					meshlet.indexCount = crr_count * 3;
					meshlet.aabbMin = DirectX::XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
					meshlet.aabbMax = DirectX::XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

					// Meshlet範囲内の頂点座標からAABBを求める
					int start = offset;
					int end = offset + meshlet.indexCount;
					for (int no = start; no < end; no++)
					{
						auto&& p = pos[idx[no]];
						meshlet.aabbMin.x = std::min(meshlet.aabbMin.x, p.x);
						meshlet.aabbMin.y = std::min(meshlet.aabbMin.y, p.y);
						meshlet.aabbMin.z = std::min(meshlet.aabbMin.z, p.z);
						meshlet.aabbMax.x = std::max(meshlet.aabbMax.x, p.x);
						meshlet.aabbMax.y = std::max(meshlet.aabbMax.y, p.y);
						meshlet.aabbMax.z = std::max(meshlet.aabbMax.z, p.z);
					}

					tmp_meshlets.push_back(meshlet);
					meshlet_count++;
					offset += meshlet.indexCount;
					tri_count -= crr_count;
				}
				meshletCounts_.push_back(meshlet_count);
			}
		};
		CreateMeshlets(glbMesh_);

		// Meshletバッファを生成する
		{
			// バッファ生成
			if (!meshletB_.Initialize(&device_, sizeof(Meshlet) * tmp_meshlets.size(), sizeof(Meshlet), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}
			memcpy(meshletB_.Map(nullptr), tmp_meshlets.data(), sizeof(Meshlet) * tmp_meshlets.size());
			meshletB_.Unmap();

			// サブメッシュごとのViewを作成
			int count = 0;
			for (auto c : meshletCounts_)
			{
				auto bv = new sl12::BufferView();
				if (!bv->Initialize(&device_, &meshletB_, count, 0, sizeof(Meshlet)))
				{
					delete bv;
					return false;
				}
				meshletBVs_.push_back(bv);
				count += c;
			}
		}

		// Argument, Counterバッファを作成
		{
			// ArgumentバッファはD3D12_DRAW_INDEXED_ARGUMENTSの構造体バッファ
			if (!indirectArgumentB_.Initialize(&device_, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * tmp_meshlets.size(), sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, false, true))
			{
				return false;
			}

			int count = 0;
			for (auto c : meshletCounts_)
			{
				auto uav = new sl12::UnorderedAccessView();
				if (!uav->Initialize(&device_, &indirectArgumentB_, count, 0, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), 0))
				{
					delete uav;
					return false;
				}
				indirectArgumentUAVs_.push_back(uav);
				count += c;
			}
		}
		{
			// Counterバッファはu32のサブメッシュ数分
			// NOTE: RWByteAddressBufferとして生成するので、16byteのアラインメントが必要
			if (!indirectCounterB_.Initialize(&device_, sizeof(sl12::u32) * 4 * meshletCounts_.size(), sizeof(sl12::u32), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, false, true))
			{
				return false;
			}

			for (int i = 0; i < meshletCounts_.size(); i++)
			{
				auto uav = new sl12::UnorderedAccessView();
				if (!uav->Initialize(&device_, &indirectCounterB_, i * 4, 0, 0, 0))
				{
					delete uav;
					return false;
				}
				indirectCounterUAVs_.push_back(uav);
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

		meshletB_.Destroy();
		for (auto&& v : meshletBVs_) sl12::SafeDelete(v);
		meshletBVs_.clear();

		indirectArgumentB_.Destroy();
		for (auto&& v : indirectArgumentUAVs_) sl12::SafeDelete(v);
		indirectArgumentUAVs_.clear();

		indirectCounterB_.Destroy();
		for (auto&& v : indirectCounterUAVs_) sl12::SafeDelete(v);
		indirectCounterUAVs_.clear();

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
		memcpy(p->frustumPlane, tmp_planes, sizeof(p->frustumPlane));
		frustumCBs_[frameIndex].Unmap();
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
	CommandLists			dxrCmdLists_;
	CommandLists			litCmdLists_;
	sl12::CommandList		utilCmdList_;
	sl12::Fence				asFence_;

	sl12::RootSignature		rtGlobalRootSig_, rtLocalRootSig_;
	sl12::RaytracingDescriptorManager	rtDescMan_;
	sl12::DescriptorSet		rtGlobalDescSet_;

	sl12::DxrPipelineState		stateObject_;
	RaytracingResult			rtResults_[kBufferCount];
	RaytracingResult			rtWork_, denoiseWork_;
	sl12::Texture				resultTexture_;
	sl12::TextureView			resultTextureSRV_;
	sl12::RenderTargetView		resultTextureRTV_;
	sl12::UnorderedAccessView	resultTextureUAV_;

	sl12::Texture				blueNoiseTex_;
	sl12::TextureView			blueNoiseSRV_;

	sl12::Buffer				randomBuffer_;
	sl12::BufferView			randomBufferSRV_;
	sl12::Buffer				seedBuffer_;
	sl12::UnorderedAccessView	seedBufferUAV_;

	sl12::GlbMesh			glbMesh_;
	sl12::Sampler			imageSampler_;

	sl12::BottomAccelerationStructure	glbBottomAS_;
	sl12::TopAccelerationStructure		topAS_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer				glbMeshCB_;
	sl12::ConstantBufferView	glbMeshCBV_;

	// multi draw indirect関連
	sl12::Buffer					meshletB_;
	std::vector<int>				meshletCounts_;
	std::vector<sl12::BufferView*>	meshletBVs_;
	sl12::Buffer							indirectArgumentB_;
	std::vector<sl12::UnorderedAccessView*>	indirectArgumentUAVs_;
	sl12::Buffer							indirectCounterB_;
	std::vector<sl12::UnorderedAccessView*>	indirectCounterUAVs_;
	sl12::Buffer					frustumCBs_[kBufferCount];
	sl12::ConstantBufferView		frustumCBVs_[kBufferCount];
	sl12::Shader					cullCS_, countClearCS_;
	sl12::RootSignature				cullRootSig_, countClearRootSig_;
	sl12::ComputePipelineState		cullPso_, countClearPso_;
	sl12::DescriptorSet				cullDescSet_;

	sl12::Buffer			rayGenTable_, missTable_, hitGroupTable_;
	sl12::u64				shaderRecordSize_;

	std::vector<Sphere>		spheres_;
	sl12::Buffer			spheresAABB_;
	sl12::Buffer			instanceSB_;
	sl12::BufferView		instanceSBV_;

	GBuffers				gbuffers_[kBufferCount];

	sl12::Shader				zpreVS_, zprePS_;
	sl12::Shader				lightingVS_, lightingPS_;
	sl12::Shader				fullscreenVS_, temporalPS_, denoiseXPS_, denoiseYPS_;
	sl12::RootSignature			zpreRootSig_, lightingRootSig_, temporalRootSig_;
	sl12::GraphicsPipelineState	zprePso_, lightingPso_, temporalPso_, denoiseXPso_, denoiseYPso_;
	ID3D12CommandSignature*		commandSig_ = nullptr;

	sl12::DescriptorSet			descSet_;

	sl12::Gui				gui_;
	sl12::InputData			inputData_{};

	sl12::Timestamp			gpuTimestamp_[sl12::Swapchain::kMaxBuffer];
	sl12::Timestamp			bottomAsTimestamp_;
	sl12::Timestamp			topAsTimestamp_[sl12::Swapchain::kMaxBuffer];

	DirectX::XMFLOAT4		camPos_ = { -5.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		upVec_ = { 0.0f, 1.0f, 0.0f, 0.0f };
	float					skyPower_ = 1.0f;
	float					lightColor_[3] = { 1.0f, 1.0f, 1.0f };
	float					lightPower_ = 1.0f;
	float					aoLength_ = 3.0f;
	int						aoSampleCount_ = 4;
	bool					isDenoise_ = true;
	uint32_t				loopCount_ = 0;
	int						randomType_ = 1;
	bool					isTemporalOn_ = true;
	bool					isAOOnly_ = false;
	int						denoiseCount_ = 1;
	bool					isClearTarget_ = true;

	DirectX::XMFLOAT4X4		mtxWorldToView_, mtxPrevWorldToView_;
	float					camRotX_ = 0.0f;
	float					camRotY_ = 0.0f;
	float					camMoveForward_ = 0.0f;
	float					camMoveLeft_ = 0.0f;
	float					camMoveUp_ = 0.0f;
	bool					isCameraMove_ = true;

	bool					isIndirectDraw_ = true;
	bool					isFreezeCull_ = false;
	DirectX::XMFLOAT4X4		mtxFrustumViewProj_;

	int		frameIndex_ = 0;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
