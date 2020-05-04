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
#include "sl12/resource_mesh.h"
#include "sl12/resource_texture.h"
#include "sl12/resource_loader.h"
#include "sl12/constant_buffer_cache.h"
#include "mesh_instance.h"
#include "as_manager.h"
#include "rtxgi_component.h"

#include "CompiledShaders/zpre.vv.hlsl.h"
#include "CompiledShaders/zpre.p.hlsl.h"
#include "CompiledShaders/lighting.p.hlsl.h"
#include "CompiledShaders/fullscreen.vv.hlsl.h"
#include "CompiledShaders/frustum_cull.c.hlsl.h"
#include "CompiledShaders/count_clear.c.hlsl.h"
#include "CompiledShaders/occlusion.lib.hlsl.h"
#include "CompiledShaders/material.lib.hlsl.h"
#include "CompiledShaders/direct_shadow.lib.hlsl.h"
#include "CompiledShaders/rt_reflection.lib.hlsl.h"
#include "CompiledShaders/probe_lighting.lib.hlsl.h"
#include "CompiledShaders/irr_map_gen.c.hlsl.h"
#include "CompiledShaders/temporal_blend.c.hlsl.h"
#include "CompiledShaders/debug_probe.vv.hlsl.h"
#include "CompiledShaders/debug_probe.p.hlsl.h"

#define USE_IN_CPP
#include "../shader/constant.h"
#undef USE_IN_CPP

#include <windowsx.h>


namespace
{
	static const int	kScreenWidth = 1280;
	static const int	kScreenHeight = 720;
	static const int	MaxSample = 512;
	static const float	kNearZ = 0.01f;
	static const float	kFarZ = 10000.0f;

	static const float	kSponzaScale = 20.0f;
	static const float	kSuzanneScale = 1.0f;

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
	static LPCWSTR kReflectionRGS = L"RTReflectionRGS";
	static LPCWSTR kReflectionMS = L"RTReflectionMS";
	static LPCWSTR kProbeLightingRGS = L"ProbeLightingRGS";
	static LPCWSTR kMaterialMS = L"MaterialMS";

	static const DXGI_FORMAT	kReytracingResultFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	static const int	kRelocationIterationCount = 50;
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
		// initialize as manager.
		pAsManager_ = new sl12::ASManager(&device_);

		// initialize constant buffer cache.
		pCBCache_ = new sl12::ConstantBufferCache();
		pCBCache_->Initialize(&device_);

		// リソースロード開始
		if (!resLoader_.Initialize(&device_))
		{
			return false;
		}
		hBlueNoiseRes_ = resLoader_.LoadRequest<sl12::ResourceItemTexture>("data/blue_noise.tga");
		hSkyHdrRes_ = resLoader_.LoadRequest<sl12::ResourceItemTexture>("data/outdoor.exr");
		//hSkyHdrRes_ = resLoader_.LoadRequest<sl12::ResourceItemTexture>("data/kloofendal_48d_partly_cloudy_4k.hdr");
		hMeshRes_[0] = resLoader_.LoadRequest<sl12::ResourceItemMesh>("data/sponza/sponza.rmesh");
		hMeshRes_[1] = resLoader_.LoadRequest<sl12::ResourceItemMesh>("data/ball/ball.rmesh");

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

		// サンプラー作成
		{
			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samDesc.MinLOD = 0.0f;
			samDesc.MaxLOD = FLT_MAX;
			if (!imageSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}
		{
			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samDesc.MaxLOD = FLT_MAX;
			samDesc.Filter = D3D12_FILTER_ANISOTROPIC;
			samDesc.MaxAnisotropy = 8;
			samDesc.MinLOD = 0.0f;
			samDesc.MaxLOD = FLT_MAX;
			if (!anisoSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}
		{
			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samDesc.MinLOD = 0.0f;
			samDesc.MaxLOD = FLT_MAX;
			if (!hdrSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}
		{
			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samDesc.MinLOD = 0.0f;
			samDesc.MaxLOD = FLT_MAX;
			if (!trilinearSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}

		if (!CreateSphere())
		{
			return false;
		}

		if (!zpreRootSig_.Initialize(&device_, &zpreVS_, &zprePS_, nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!lightingRootSig_.Initialize(&device_, &fullscreenVS_, &lightingPS_, nullptr, nullptr, nullptr))
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

			desc.rasterizer.cullMode = D3D12_CULL_MODE_BACK;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = true;
			desc.depthStencil.isDepthWriteEnable = true;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

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

			if (!zprePso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!fullscreenVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pFullscreenVS, sizeof(g_pFullscreenVS)))
			{
				return false;
			}
			if (!lightingPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pLightingPS, sizeof(g_pLightingPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &lightingRootSig_;
			desc.pVS = &fullscreenVS_;
			desc.pPS = &lightingPS_;

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
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.multisampleCount = 1;

			if (!lightingPso_.Initialize(&device_, desc))
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
		{
			if (!debugProbeVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pDebugProbeVS, sizeof(g_pDebugProbeVS)))
			{
				return false;
			}
			if (!debugProbePS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pDebugProbePS, sizeof(g_pDebugProbePS)))
			{
				return false;
			}
			if (!debugProbeRS_.Initialize(&device_, &debugProbeVS_, &debugProbePS_, nullptr, nullptr, nullptr))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &debugProbeRS_;
			desc.pVS = &debugProbeVS_;
			desc.pPS = &debugProbePS_;

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
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			desc.inputLayout.numElements = ARRAYSIZE(input_elems);
			desc.inputLayout.pElements = input_elems;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.multisampleCount = 1;

			if (!debugProbePso_.Initialize(&device_, desc))
			{
				return false;
			}
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

			rtxgiComponent_.reset(new RtxgiComponent(&device_));
			if (!rtxgiComponent_->Initialize())
			{
				assert(!"FAILED!! initialize rtxgi.");
			}

			InitializeRaytracingPipeline();
			utilCmdList_.Reset();
			InitializeIrradianceMap(&utilCmdList_);
			CreateAS(&utilCmdList_);
			utilCmdList_.Close();
			utilCmdList_.Execute();
			device_.WaitDrawDone();
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
		auto d3dCmdList = pCmdList->GetCommandList();
		auto&& curGBuffer = gbuffers_[frameIndex];
		auto&& prevGBuffer = gbuffers_[prevFrameIndex];

		UpdateSceneCB(frameIndex);
		rtxgiComponent_->UpdateVolume(nullptr);

		pCBCache_->BeginNewFrame();
		gui_.BeginNewFrame(&litCmdList, kScreenWidth, kScreenHeight, inputData_);

		// カメラ操作
		ControlCamera();
		if (isCameraMove_)
		{
			isCameraMove_ = false;
		}

		// GUI
		{
			if (ImGui::SliderFloat("Sky Power", &skyPower_, 0.0f, 10.0f))
			{
			}
			if (ImGui::SliderFloat("Light Intensity", &lightIntensity_, 0.0f, 10.0f))
			{
			}
			if (ImGui::ColorEdit3("Light Color", lightColor_))
			{
			}
			if (ImGui::SliderFloat("Spot Intensity", &spotLightIntensity_, 0.0f, 1000.0f))
			{
			}
			if (ImGui::SliderFloat("Spot Cone Angle", &spotLightOuterAngle_, 10.0f, 50.0f))
			{
			}
			if (ImGui::ColorEdit3("Spot Color", spotLightColor_))
			{
			}
			if (ImGui::SliderFloat("GI Intensity", &giIntensity_, 0.0f, 10.0f))
			{
			}
			if (ImGui::SliderFloat("Reflection Intensity", &reflectionIntensity_, 0.0f, 10.0f))
			{
			}
			if (ImGui::SliderFloat("Reflection Blend Weight", &reflectionBlend_, 0.001f, 1.0f))
			{
			}
			if (ImGui::SliderFloat("Reflection Roughness Max", &reflectionRoughnessMax_, 0.01f, 1.0f))
			{
			}
			if (ImGui::SliderFloat2("Roughness Range", (float*)&roughnessRange_, 0.0f, 1.0f))
			{
			}
			if (ImGui::SliderFloat2("Metallic Range", (float*)&metallicRange_, 0.0f, 1.0f))
			{
			}
			float hysteresis = rtxgiComponent_->GetDDGIVolume()->GetProbeHysteresis();
			if (ImGui::SliderFloat("Hysteresis", &hysteresis, 0.0f, 1.0f))
			{
				rtxgiComponent_->SetDescHysteresis(hysteresis);
			}
			float changeT = rtxgiComponent_->GetDDGIVolume()->GetProbeChangeThreshold();
			if (ImGui::SliderFloat("Change Threshold", &changeT, 0.0f, 1.0f))
			{
				rtxgiComponent_->SetDescChangeThreshold(changeT);
			}
			float brightT = rtxgiComponent_->GetDDGIVolume()->GetProbeBrightnessThreshold();
			if (ImGui::SliderFloat("Brightness Threshold", &brightT, 0.0f, 10.0f))
			{
				rtxgiComponent_->SetDescBrightnessThreshold(brightT);
			}
			if (ImGui::Button("Probe Relocation"))
			{
				ddgiRelocationCount_ = kRelocationIterationCount;
			}
			ImGui::Checkbox("Indirect Draw", &isIndirectDraw_);
			ImGui::Checkbox("Freeze Cull", &isFreezeCull_);
			ImGui::Checkbox("Stop Spot", &isStopSpot_);
			ImGui::Checkbox("Display Probe", &isDebugProbe_);

			uint64_t freq = device_.GetGraphicsQueue().GetTimestampFrequency();
			uint64_t timestamp[6];

			gpuTimestamp_[frameIndex].GetTimestamp(0, 6, timestamp);
			uint64_t all_time = timestamp[2] - timestamp[0];
			float all_ms = (float)all_time / ((float)freq / 1000.0f);

			ImGui::Text("All GPU: %f (ms)", all_ms);
		}

		// update constant buffers.
		sl12::ConstantBufferCache::Handle hReflectionCB;
		{
			auto noise = hBlueNoiseRes_.GetItem<sl12::ResourceItemTexture>();

			ReflectionCB cb;
			cb.intensity = reflectionIntensity_;
			cb.currentBlendMax = reflectionBlend_;
			cb.roughnessMax = reflectionRoughnessMax_;
			cb.noiseTexWidth = (UINT)noise->GetTexture().GetResourceDesc().Width;
			cb.time = frameTime_ % reflectionFrameMax_;
			cb.timeMax = reflectionFrameMax_;
			hReflectionCB = pCBCache_->GetUnusedConstBuffer(sizeof(cb), &cb);
		}
		frameTime_++;

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

		// ceate mesh cb.
		struct RenderMeshComponent
		{
			sl12::MeshInstance*					pInstance;
			sl12::ConstantBufferCache::Handle	cbHandle;

			RenderMeshComponent(sl12::MeshInstance* p)
				: pInstance(p)
			{}
		};
		RenderMeshComponent meshes[] = {
			RenderMeshComponent(&sponzaInstance_),
			//RenderMeshComponent(&ballInstance_)
		};
		for (auto&& mesh : meshes)
		{
			MeshCB cb;
			cb.mtxLocalToWorld = cb.mtxPrevLocalToWorld = mesh.pInstance->GetMtxTransform();
			mesh.cbHandle = pCBCache_->GetUnusedConstBuffer(sizeof(cb), &cb);
		}

		// GPU culling
		if (isIndirectDraw_)
		{
			// PSO設定
			d3dCmdList->SetPipelineState(cullPso_.GetPSO());

			// 基本Descriptor設定
			cullDescSet_.Reset();
			cullDescSet_.SetCsCbv(0, frustumCBVs_[frameIndex].GetDescInfo().cpuHandle);

			auto Dispatch = [&](RenderMeshComponent& mesh)
			{
				auto&& meshlets = mesh.pInstance->GetMeshletComponents();
				if (meshlets.empty())
				{
					return;
				}

				cullDescSet_.SetCsCbv(1, mesh.cbHandle.GetCBV()->GetDescInfo().cpuHandle);

				auto pMesh = mesh.pInstance->GetResMesh().GetItem<sl12::ResourceItemMesh>();
				auto&& submeshes = pMesh->GetSubmeshes();
				auto submesh_count = submeshes.size();
				for (int i = 0; i < submesh_count; i++)
				{
					int dispatch_x = (int)submeshes[i].meshlets.size();
					auto&& bv = meshlets[i]->GetMeshletBV();
					auto&& arg_uav = meshlets[i]->GetIndirectArgumentUAV();

					cullDescSet_.SetCsSrv(0, bv.GetDescInfo().cpuHandle);
					cullDescSet_.SetCsUav(0, arg_uav.GetDescInfo().cpuHandle);

					pCmdList->SetComputeRootSignatureAndDescriptorSet(&cullRootSig_, &cullDescSet_);

					pCmdList->TransitionBarrier(&meshlets[i]->GetIndirectArgumentB(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					d3dCmdList->Dispatch(dispatch_x, 1, 1);
				}
			};
			auto FinishBarrier = [&](RenderMeshComponent& mesh)
			{
				auto&& meshlets = mesh.pInstance->GetMeshletComponents();
				for (auto&& meshlet : meshlets)
				{
					pCmdList->TransitionBarrier(&meshlet->GetIndirectArgumentB(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
				}
			};

			for (auto&& mesh : meshes)
			{
				Dispatch(mesh);
			}
			for (auto&& mesh : meshes)
			{
				FinishBarrier(mesh);
			}
		}

		D3D12_SHADING_RATE_COMBINER shading_rate_combiners[] = {
			D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
			D3D12_SHADING_RATE_COMBINER_OVERRIDE,
		};

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
			descSet_.SetPsCbv(1, materialCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, anisoSampler_.GetDescInfo().cpuHandle);

			auto RenderMesh = [&](RenderMeshComponent& mesh)
			{
				auto&& meshlets = mesh.pInstance->GetMeshletComponents();
				bool bIndirectDraw = isIndirectDraw_ && !meshlets.empty();

				descSet_.SetVsCbv(1, mesh.cbHandle.GetCBV()->GetDescInfo().cpuHandle);

				auto pMesh = mesh.pInstance->GetResMesh().GetItem<sl12::ResourceItemMesh>();
				auto&& submeshes = pMesh->GetSubmeshes();
				auto submesh_count = submeshes.size();
				for (int i = 0; i < submesh_count; i++)
				{
					auto&& submesh = submeshes[i];
					auto&& material = pMesh->GetMaterials()[submesh.materialIndex];
					auto bc_res = const_cast<sl12::ResourceItemTexture*>(material.baseColorTex.GetItem<sl12::ResourceItemTexture>());
					auto n_res = const_cast<sl12::ResourceItemTexture*>(material.normalTex.GetItem<sl12::ResourceItemTexture>());
					auto orm_res = const_cast<sl12::ResourceItemTexture*>(material.ormTex.GetItem<sl12::ResourceItemTexture>());

					descSet_.SetPsSrv(0, bc_res->GetTextureView().GetDescInfo().cpuHandle);
					descSet_.SetPsSrv(1, n_res->GetTextureView().GetDescInfo().cpuHandle);
					descSet_.SetPsSrv(2, orm_res->GetTextureView().GetDescInfo().cpuHandle);
					pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&zpreRootSig_, &descSet_);

					const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
						submesh.positionVBV.GetView(),
						submesh.normalVBV.GetView(),
						submesh.tangentVBV.GetView(),
						submesh.texcoordVBV.GetView(),
					};
					d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

					auto&& ibv = submesh.indexBV.GetView();
					d3dCmdList->IASetIndexBuffer(&ibv);

					if (bIndirectDraw)
					{
						d3dCmdList->ExecuteIndirect(
							commandSig_,											// command signature
							(UINT)submesh.meshlets.size(),							// コマンドの最大発行回数
							meshlets[i]->GetIndirectArgumentB().GetResourceDep(),	// indirectコマンドの変数バッファ
							0,														// indirectコマンドの変数バッファの先頭オフセット
							nullptr,												// 実際の発行回数を収めたカウントバッファ
							0);														// カウントバッファの先頭オフセット
					}
					else
					{
						d3dCmdList->DrawIndexedInstanced(submesh.indexCount, 1, 0, 0, 0);
					}
				}
			};

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			for (auto&& mesh : meshes)
			{
				RenderMesh(mesh);
			}
		}

		// リソースバリア
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[1], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.gbufferTex[2], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

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
				pAsManager_->GetTlas()->GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress(),
			};
			pCmdList->SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, pRtDescManager_, as_address, ARRAYSIZE(as_address));

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

		// probe lighting.
		{
			// デスクリプタを設定
			rtGlobalDescSet_.Reset();
			rtGlobalDescSet_.SetCsCbv(0, rtxgiComponent_->GetCurrentVolumeCBV()->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(1, rtxgiComponent_->GetIrradianceSRV()->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(2, rtxgiComponent_->GetDistanceSRV()->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(3, irrTexSrv_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(0, rtxgiComponent_->GetRadianceUAV()->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(1, rtxgiComponent_->GetOffsetUAV()->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(2, rtxgiComponent_->GetStateUAV()->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSampler(0, trilinearSampler_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSampler(1, hdrSampler_.GetDescInfo().cpuHandle);

			// コピーしつつコマンドリストに積む
			D3D12_GPU_VIRTUAL_ADDRESS as_address[] = {
				pAsManager_->GetTlas()->GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress(),
			};
			pCmdList->SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, pRtDescManager_, as_address, ARRAYSIZE(as_address));

			// レイトレースを実行
			D3D12_DISPATCH_RAYS_DESC desc{};
			desc.HitGroupTable.StartAddress = rtProbeLightingHGTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.HitGroupTable.SizeInBytes = rtProbeLightingHGTable_.GetSize();
			desc.HitGroupTable.StrideInBytes = rtShaderRecordSize_;
			desc.MissShaderTable.StartAddress = rtProbeLightingMSTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.MissShaderTable.SizeInBytes = rtProbeLightingMSTable_.GetSize();
			desc.MissShaderTable.StrideInBytes = rtShaderRecordSize_;
			desc.RayGenerationShaderRecord.StartAddress = rtProbeLightingRGSTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = rtProbeLightingRGSTable_.GetSize();
			desc.Width = rtxgiComponent_->GetDDGIVolume()->GetNumRaysPerProbe();
			desc.Height = rtxgiComponent_->GetDDGIVolume()->GetNumProbes();
			desc.Depth = 1;
			pCmdList->GetDxrCommandList()->SetPipelineState1(rtProbeLightingPSO_.GetPSO());
			pCmdList->GetDxrCommandList()->DispatchRays(&desc);

			// uav barrier.
			pCmdList->UAVBarrier(rtxgiComponent_->GetRadiance());

			// update probes.
			rtxgiComponent_->UpdateProbes(pCmdList);

			// relocate probes.
			if (ddgiRelocationCount_ > 0)
			{
				float distanceScale = (float)ddgiRelocationCount_ / (float)kRelocationIterationCount;
				rtxgiComponent_->RelocateProbes(pCmdList, distanceScale);
				ddgiRelocationCount_--;
			}

			if (ddgiRelocationCount_ <= 0)
			{
				rtxgiComponent_->ClassifyProbes(pCmdList);
			}

			pCmdList->SetDescriptorHeapDirty();
		}

		// raytrace reflection.
		RaytracingResult* pPrevReflResult = nullptr;
		RaytracingResult* pCurrReflResult = nullptr;
		if (rtCurrentReflectionResultIndex_ >= 0)
		{
			pPrevReflResult = &rtReflectionResults_[1 - rtCurrentReflectionResultIndex_];
			pCurrReflResult = &rtReflectionResults_[rtCurrentReflectionResultIndex_];
			rtCurrentReflectionResultIndex_ = 1 - rtCurrentReflectionResultIndex_;
		}
		else
		{
			pCurrReflResult = &rtReflectionResults_[0];
			rtCurrentReflectionResultIndex_ = 1;
		}
		{
			pCmdList->TransitionBarrier(&pCurrReflResult->tex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// デスクリプタを設定
			auto sky = const_cast<sl12::ResourceItemTexture*>(hSkyHdrRes_.GetItem<sl12::ResourceItemTexture>());
			auto noise = const_cast<sl12::ResourceItemTexture*>(hBlueNoiseRes_.GetItem<sl12::ResourceItemTexture>());
			rtGlobalDescSet_.Reset();
			rtGlobalDescSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsCbv(2, hReflectionCB.GetCBV()->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(1, curGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(2, curGBuffer.gbufferSRV[2].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(3, curGBuffer.depthSRV.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(4, irrTexSrv_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(5, sky->GetTextureView().GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(6, noise->GetTextureView().GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(0, pCurrReflResult->uav.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSampler(0, hdrSampler_.GetDescInfo().cpuHandle);

			// コピーしつつコマンドリストに積む
			D3D12_GPU_VIRTUAL_ADDRESS as_address[] = {
				pAsManager_->GetTlas()->GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress(),
			};
			pCmdList->SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, pRtDescManager_, as_address, ARRAYSIZE(as_address));

			// レイトレースを実行
			D3D12_DISPATCH_RAYS_DESC desc{};
			desc.HitGroupTable.StartAddress = rtReflectionHGTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.HitGroupTable.SizeInBytes = rtReflectionHGTable_.GetSize();
			desc.HitGroupTable.StrideInBytes = rtShaderRecordSize_;
			desc.MissShaderTable.StartAddress = rtReflectionMSTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.MissShaderTable.SizeInBytes = rtReflectionMSTable_.GetSize();
			desc.MissShaderTable.StrideInBytes = rtShaderRecordSize_;
			desc.RayGenerationShaderRecord.StartAddress = rtReflectionRGSTable_.GetResourceDep()->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = rtReflectionRGSTable_.GetSize();
			desc.Width = kScreenWidth;
			desc.Height = kScreenHeight;
			desc.Depth = 1;
			pCmdList->GetDxrCommandList()->SetPipelineState1(rtReflectionPSO_.GetPSO());
			pCmdList->GetDxrCommandList()->DispatchRays(&desc);

			// temporal blend.
			if (pPrevReflResult)
			{
				d3dCmdList->SetPipelineState(temporalBlendPso_.GetPSO());

				cullDescSet_.Reset();
				cullDescSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
				cullDescSet_.SetCsCbv(1, hReflectionCB.GetCBV()->GetDescInfo().cpuHandle);
				cullDescSet_.SetCsSrv(0, pPrevReflResult->srv.GetDescInfo().cpuHandle);
				cullDescSet_.SetCsSrv(1, curGBuffer.gbufferSRV[2].GetDescInfo().cpuHandle);
				cullDescSet_.SetCsSrv(2, curGBuffer.depthSRV.GetDescInfo().cpuHandle);
				cullDescSet_.SetCsSrv(3, prevGBuffer.depthSRV.GetDescInfo().cpuHandle);
				cullDescSet_.SetCsUav(0, pCurrReflResult->uav.GetDescInfo().cpuHandle);
				cullDescSet_.SetCsSampler(0, hdrSampler_.GetDescInfo().cpuHandle);

				pCmdList->SetComputeRootSignatureAndDescriptorSet(&temporalBlendRS_, &cullDescSet_);
				d3dCmdList->Dispatch((kScreenWidth + 7) / 8, (kScreenHeight + 7) / 8, 1);
			}

			pCmdList->TransitionBarrier(&pCurrReflResult->tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		// コマンドリスト変更
		pCmdList = &litCmdList;
		d3dCmdList = pCmdList->GetCommandList();

		pCmdList->SetDescriptorHeapDirty();

		// lighting pass
		{
			// set rendar target.
			auto&& rtv = swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle;
			auto&& dsv = curGBuffer.depthDSV.GetDescInfo().cpuHandle;
			d3dCmdList->OMSetRenderTargets(1, &rtv, false, &dsv);

			// set viewport and scissor rect.
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
			descSet_.SetPsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(2, hReflectionCB.GetCBV()->GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(3, rtxgiComponent_->GetCurrentVolumeCBV()->GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(0, curGBuffer.gbufferSRV[0].GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(1, curGBuffer.gbufferSRV[1].GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(2, curGBuffer.gbufferSRV[2].GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(3, curGBuffer.depthSRV.GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(4, rtShadowResult_.srv.GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(5, pCurrReflResult->srv.GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(6, irrTexSrv_.GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(7, rtxgiComponent_->GetIrradianceSRV()->GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(8, rtxgiComponent_->GetDistanceSRV()->GetDescInfo().cpuHandle);
			descSet_.SetPsUav(0, rtxgiComponent_->GetOffsetUAV()->GetDescInfo().cpuHandle);
			descSet_.SetPsUav(1, rtxgiComponent_->GetStateUAV()->GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, anisoSampler_.GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(1, hdrSampler_.GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(2, trilinearSampler_.GetDescInfo().cpuHandle);

			pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&lightingRootSig_, &descSet_);
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3dCmdList->IASetIndexBuffer(nullptr);
			d3dCmdList->DrawInstanced(3, 1, 0, 0);
		}

		// display probe.
		if (isDebugProbe_)
		{
			// PSO設定
			d3dCmdList->SetPipelineState(debugProbePso_.GetPSO());

			// 基本Descriptor設定
			descSet_.Reset();
			descSet_.SetVsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetVsCbv(1, rtxgiComponent_->GetCurrentVolumeCBV()->GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(1, lightCBVs_[frameIndex].GetDescInfo().cpuHandle);
			descSet_.SetPsCbv(2, rtxgiComponent_->GetCurrentVolumeCBV()->GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(0, rtxgiComponent_->GetIrradianceSRV()->GetDescInfo().cpuHandle);
			descSet_.SetPsSrv(1, rtxgiComponent_->GetDistanceSRV()->GetDescInfo().cpuHandle);
			descSet_.SetPsUav(0, rtxgiComponent_->GetOffsetUAV()->GetDescInfo().cpuHandle);
			descSet_.SetPsUav(1, rtxgiComponent_->GetStateUAV()->GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, trilinearSampler_.GetDescInfo().cpuHandle);
			pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&debugProbeRS_, &descSet_);

			const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
				sphereVBV_.GetView(),
			};
			d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

			auto&& ibv = sphereIBV_.GetView();
			d3dCmdList->IASetIndexBuffer(&ibv);

			d3dCmdList->DrawIndexedInstanced(sphereIndexCount_, rtxgiComponent_->GetDDGIVolume()->GetNumProbes(), 0, 0, 0);
		}

		ImGui::Render();

		pCmdList->TransitionBarrier(&curGBuffer.depthTex, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		gpuTimestamp_[frameIndex].Query(pCmdList);
		gpuTimestamp_[frameIndex].Resolve(pCmdList);

		// コマンド終了と描画待ち
		zpreCmdLists_.Close();
		litCmdLists_.Close();
		device_.WaitDrawDone();

		// 次のフレームへ
		device_.Present(1);

		// コマンド実行
		zpreCmdLists_.Execute();
		litCmdLists_.Execute();
	}

	void Finalize() override
	{
		// 描画待ち
		device_.WaitDrawDone();
		device_.Present(1);

		sl12::SafeDelete(pAsManager_);
		sl12::SafeDelete(pCBCache_);

		DestroyIrradianceMap();
		DestroyRaytracing();

		imageSampler_.Destroy();
		anisoSampler_.Destroy();
		hdrSampler_.Destroy();
		trilinearSampler_.Destroy();

		glbMeshCBV_.Destroy();
		glbMeshCB_.Destroy();

		for (auto&& v : sceneCBVs_) v.Destroy();
		for (auto&& v : sceneCBs_) v.Destroy();

		for (auto&& v : lightCBVs_) v.Destroy();
		for (auto&& v : lightCBs_) v.Destroy();

		for (auto&& v : materialCBVs_) v.Destroy();
		for (auto&& v : materialCBs_) v.Destroy();

		for (auto&& v : gpuTimestamp_) v.Destroy();

		gui_.Destroy();

		DestroyIndirectDrawParams();

		for (auto&& v : gbuffers_) v.Destroy();

		countClearPso_.Destroy();
		countClearCS_.Destroy();
		cullPso_.Destroy();
		cullCS_.Destroy();
		fullscreenVS_.Destroy();
		lightingPso_.Destroy();
		lightingPS_.Destroy();
		zprePso_.Destroy();
		zpreVS_.Destroy();
		zprePS_.Destroy();

		countClearRootSig_.Destroy();
		cullRootSig_.Destroy();
		lightingRootSig_.Destroy();
		zpreRootSig_.Destroy();

		utilCmdList_.Destroy();
		litCmdLists_.Destroy();
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
	bool CreateSceneCB()
	{
		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			DirectX::XMLoadFloat4(&camPos_),
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, kNearZ, kFarZ);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMStoreFloat4x4(&mtxWorldToView_, mtxWorldToView);
		mtxPrevWorldToView_ = mtxWorldToView_;

		spotLightRotation_ = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, DirectX::XMConvertToRadians(60.0));

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
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, kNearZ, kFarZ);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);
		auto mtxPrevWorldToClip = mtxPrevWorldToView * mtxViewToClip;
		auto mtxPrevClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxPrevWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 0.1f, -1.0f, 0.1f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));
		DirectX::XMFLOAT4 lightColor = { lightColor_[0] * lightIntensity_, lightColor_[1] * lightIntensity_, lightColor_[2] * lightIntensity_, 1.0f };

		DirectX::XMFLOAT4 spotLightColor = { spotLightColor_[0] * spotLightIntensity_, spotLightColor_[1] * spotLightIntensity_, spotLightColor_[2] * spotLightIntensity_, 1.0f };
		DirectX::XMFLOAT4 spotLightPosAndRadius(spotLightPosition_.x, spotLightPosition_.y, spotLightPosition_.z, spotLightRadius_);
		DirectX::XMFLOAT4 spotLightDirBase(0.0f, -1.0f, 0.0f, 0.0f);
		auto spotLightDir = DirectX::XMLoadFloat4(&spotLightDirBase);
		spotLightDir = DirectX::XMVector3Rotate(spotLightDir, spotLightRotation_);
		DirectX::XMFLOAT4 spotLightDirAndCos;
		DirectX::XMStoreFloat4(&spotLightDirAndCos, spotLightDir);
		spotLightDirAndCos.w = cosf(DirectX::XMConvertToRadians(spotLightOuterAngle_));

		if (!isStopSpot_)
		{
			auto rotQuat = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XMConvertToRadians(1.0f), 0.0f);
			spotLightRotation_ = DirectX::XMQuaternionMultiply(spotLightRotation_, rotQuat);
		}

		{
			auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[frameIndex].Map(nullptr));
			DirectX::XMStoreFloat4x4(&cb->mtxWorldToProj, mtxWorldToClip);
			DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
			DirectX::XMStoreFloat4x4(&cb->mtxPrevWorldToProj, mtxPrevWorldToClip);
			DirectX::XMStoreFloat4x4(&cb->mtxPrevProjToWorld, mtxPrevClipToWorld);
			cb->screenInfo.x = kNearZ;
			cb->screenInfo.y = kFarZ;
			cb->screenInfo.z = kScreenWidth;
			cb->screenInfo.w = kScreenHeight;
			DirectX::XMStoreFloat4(&cb->camPos, cp);
			sceneCBs_[frameIndex].Unmap();
		}

		{
			auto cb = reinterpret_cast<LightCB*>(lightCBs_[frameIndex].Map(nullptr));
			cb->lightDir = lightDir;
			cb->lightColor = lightColor;
			cb->spotLightColor = spotLightColor;
			cb->spotLightPosAndRadius = spotLightPosAndRadius;
			cb->spotLightDirAndCos = spotLightDirAndCos;
			cb->skyPower = skyPower_;
			cb->giIntensity = giIntensity_;
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

	bool CreateBottomAS(sl12::CommandList* pCmdList, const sl12::ResourceItemMesh* pMeshItem, sl12::BottomAccelerationStructure* pBottomAS)
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
		if (!bottomInput.InitializeAsBottom(&device_, geoDescs.data(), (UINT)submeshes.size(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		if (!pBottomAS->CreateBuffer(&device_, bottomInput.prebuildInfo.ResultDataMaxSizeInBytes, bottomInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}

		// コマンド発行
		if (!pBottomAS->Build(pCmdList, bottomInput))
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
		if (!pTopAS->Build(pCmdList, topInput))
		{
			return false;
		}

		return true;
	}

	std::vector<const sl12::ResourceItemMesh*> GetBaseMeshes()
	{
		std::vector<const sl12::ResourceItemMesh*> ret;

		ret.push_back(hMeshRes_[0].GetItem<sl12::ResourceItemMesh>());

		return ret;
	}

	bool CreateAS(sl12::CommandList* pCmdList)
	{
		// as manager.
		{
			// create mesh instance.
			DirectX::XMFLOAT4X4 mtxSponza(
				kSponzaScale, 0, 0, 0,
				0, kSponzaScale, 0, 0,
				0, 0, kSponzaScale, 0,
				0, 0, 0, 1);
			if (!sponzaInstance_.Initialize(&device_, hMeshRes_[0]))
			{
				return false;
			}
			sponzaInstance_.SetMtxTransform(mtxSponza);

			DirectX::XMFLOAT4X4 mtxBall(
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, -4, 0, 1);
			if (!ballInstance_.Initialize(&device_, hMeshRes_[1]))
			{
				return false;
			}
			ballInstance_.SetMtxTransform(mtxBall);

			// build as.
			sl12::TlasInstance instances[] = {
				{&sponzaInstance_, 0xff, 0},
				//{&ballInstance_, 0xff, 0},
			};
			pAsManager_->EntryMeshItem(hMeshRes_[0]);
			pAsManager_->EntryMeshItem(hMeshRes_[1]);
			if (!pAsManager_->Build(pCmdList, instances, ARRAYSIZE(instances), kRTMaterialTableCount))
			{
				return false;
			}

			// initialize descriptor manager.
			pRtDescManager_ = new sl12::RaytracingDescriptorManager();
			if (!pRtDescManager_->Initialize(&device_,
				2,		// renderCount
				1,		// asCount,
				4,		// globalCbvCount
				8,		// globalSrvCount
				4,		// globalUavCount
				4,		// globalSamplerCount
				pAsManager_->GetTotalMaterialCount()))
			{
				return false;
			}
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
			D3D12_EXPORT_DESC libExport[] = {
				{ kOcclusionCHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kOcclusionAHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(g_pOcclusionLib, sizeof(g_pOcclusionLib), libExport, ARRAYSIZE(libExport));

			// hit group.
			dxrDesc.AddHitGroup(kOcclusionOpacityHG, true, nullptr, kOcclusionCHS, nullptr);
			dxrDesc.AddHitGroup(kOcclusionMaskedHG, true, kOcclusionAHS, kOcclusionCHS, nullptr);

			// local root signature.
			// if use only one root signature, do not need export association.
			dxrDesc.AddLocalRootSignatureAndExportAssociation(rtLocalRootSig_, nullptr, 0);

			// RaytracingPipelineConfigをリンク先で設定するため、このフラグが必要
			dxrDesc.AddStateObjectConfig(D3D12_STATE_OBJECT_FLAG_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITIONS);

			// PSO生成
			if (!rtOcclusionCollection_.Initialize(&device_, dxrDesc, D3D12_STATE_OBJECT_TYPE_COLLECTION))
			{
				return false;
			}
		}
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			// export shader from library.
			D3D12_EXPORT_DESC libExport[] = {
				{ kMaterialCHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kMaterialAHS,	nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(g_pMaterialLib, sizeof(g_pMaterialLib), libExport, ARRAYSIZE(libExport));

			// hit group.
			dxrDesc.AddHitGroup(kMaterialOpacityHG, true, nullptr, kMaterialCHS, nullptr);
			dxrDesc.AddHitGroup(kMaterialMaskedHG, true, kMaterialAHS, kMaterialCHS, nullptr);

			// local root signature.
			// if use only one root signature, do not need export association.
			dxrDesc.AddLocalRootSignatureAndExportAssociation(rtLocalRootSig_, nullptr, 0);

			// RaytracingPipelineConfigをリンク先で設定するため、このフラグが必要
			dxrDesc.AddStateObjectConfig(D3D12_STATE_OBJECT_FLAG_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITIONS);

			// PSO生成
			if (!rtMaterialCollection_.Initialize(&device_, dxrDesc, D3D12_STATE_OBJECT_TYPE_COLLECTION))
			{
				return false;
			}
		}

		// create pipeline state.
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			// export shader from library.
			D3D12_EXPORT_DESC libExport[] = {
				{ kDirectShadowRGS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kDirectShadowMS,	nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(g_pDirectShadowLib, sizeof(g_pDirectShadowLib), libExport, ARRAYSIZE(libExport));

			// payload size and intersection attr size.
			//dxrDesc.AddShaderConfig(16, sizeof(float) * 2);
			dxrDesc.AddShaderConfig(sizeof(float) * 1, sizeof(float) * 2);

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
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			// export shader from library.
			D3D12_EXPORT_DESC libExport[] = {
				{ kReflectionMS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kReflectionRGS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kDirectShadowMS,	nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(g_pRTReflectionLib, sizeof(g_pRTReflectionLib), libExport, ARRAYSIZE(libExport));

			// payload size and intersection attr size.
			dxrDesc.AddShaderConfig(16, sizeof(float) * 2);

			// global root signature.
			dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);

			// TraceRay recursive count.
			dxrDesc.AddRaytracinConfig(1);

			// hit group collection.
			dxrDesc.AddExistingCollection(rtMaterialCollection_.GetPSO(), nullptr, 0);
			dxrDesc.AddExistingCollection(rtOcclusionCollection_.GetPSO(), nullptr, 0);

			// PSO生成
			if (!rtReflectionPSO_.Initialize(&device_, dxrDesc))
			{
				return false;
			}
		}
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			// export shader from library.
			D3D12_EXPORT_DESC libExport[] = {
				{ kProbeLightingRGS,	nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kMaterialMS,			nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kDirectShadowMS,		nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(g_pProbeLightingLib, sizeof(g_pProbeLightingLib), libExport, ARRAYSIZE(libExport));

			// payload size and intersection attr size.
			dxrDesc.AddShaderConfig(16, sizeof(float) * 2);

			// global root signature.
			dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);

			// TraceRay recursive count.
			dxrDesc.AddRaytracinConfig(1);

			// hit group collection.
			dxrDesc.AddExistingCollection(rtMaterialCollection_.GetPSO(), nullptr, 0);
			dxrDesc.AddExistingCollection(rtOcclusionCollection_.GetPSO(), nullptr, 0);

			// PSO生成
			if (!rtProbeLightingPSO_.Initialize(&device_, dxrDesc))
			{
				return false;
			}
		}

		// temporal blend pipeline.
		{
			if (!temporalBlendCS_.Initialize(&device_, sl12::ShaderType::Compute, g_pTemporalBlendCS, sizeof(g_pTemporalBlendCS)))
			{
				return false;
			}

			if (!temporalBlendRS_.Initialize(&device_, &temporalBlendCS_))
			{
				return false;
			}

			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &temporalBlendRS_;
			desc.pCS = &temporalBlendCS_;

			if (!temporalBlendPso_.Initialize(&device_, desc))
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
			desc.format = DXGI_FORMAT_R8G8_UNORM;
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
			desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.depth = 1;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.mipLevels = 1;
			desc.sampleCount = 1;
			desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.isRenderTarget = true;
			desc.isUav = true;

			for (auto&& v : rtReflectionResults_)
			{
				if (!v.Initialize(&device_, desc))
				{
					return false;
				}
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

		{
			struct MaterialTable
			{
				LocalTable		table;
				bool			opaque;
			};
			std::vector<MaterialTable> material_table;
			auto view_desc_size = pRtDescManager_->GetViewDescSize();
			auto sampler_desc_size = pRtDescManager_->GetSamplerDescSize();
			auto local_handle_start = pRtDescManager_->IncrementLocalHandleStart();
			auto SetMaterialFunc = [&](const sl12::BlasItem* item)
			{
				auto pMeshItem = item->GetMeshItem();
				auto&& submeshes = pMeshItem->GetSubmeshes();
				for (int i = 0; i < submeshes.size(); i++)
				{
					auto&& submesh = submeshes[i];
					auto&& material = pMeshItem->GetMaterials()[submesh.materialIndex];
					auto pTexBC = material.baseColorTex.GetItem<sl12::ResourceItemTexture>();
					auto pTexORM = material.ormTex.GetItem<sl12::ResourceItemTexture>();

					MaterialTable table;
					table.opaque = material.isOpaque;

					// CBV
					table.table.cbv = local_handle_start.viewGpuHandle;

					// SRV
					D3D12_CPU_DESCRIPTOR_HANDLE srv[] = {
						submesh.indexView.GetDescInfo().cpuHandle,
						submesh.normalView.GetDescInfo().cpuHandle,
						submesh.texcoordView.GetDescInfo().cpuHandle,
						const_cast<sl12::ResourceItemTexture*>(pTexBC)->GetTextureView().GetDescInfo().cpuHandle,
						const_cast<sl12::ResourceItemTexture*>(pTexORM)->GetTextureView().GetDescInfo().cpuHandle,
					};
					sl12::u32 srv_cnt = ARRAYSIZE(srv);
					device_.GetDeviceDep()->CopyDescriptors(
						1, &local_handle_start.viewCpuHandle, &srv_cnt,
						srv_cnt, srv, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					table.table.srv = local_handle_start.viewGpuHandle;
					local_handle_start.viewCpuHandle.ptr += view_desc_size * srv_cnt;
					local_handle_start.viewGpuHandle.ptr += view_desc_size * srv_cnt;

					// UAVはなし
					table.table.uav = local_handle_start.viewGpuHandle;

					// Samplerは1つ
					D3D12_CPU_DESCRIPTOR_HANDLE sampler[] = {
						imageSampler_.GetDescInfo().cpuHandle,
					};
					sl12::u32 sampler_cnt = ARRAYSIZE(sampler);
					device_.GetDeviceDep()->CopyDescriptors(
						1, &local_handle_start.samplerCpuHandle, &sampler_cnt,
						sampler_cnt, sampler, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
					table.table.sampler = local_handle_start.samplerGpuHandle;
					local_handle_start.samplerCpuHandle.ptr += sampler_desc_size * sampler_cnt;
					local_handle_start.samplerGpuHandle.ptr += sampler_desc_size * sampler_cnt;

					material_table.push_back(table);
				}
				return true;
			};

			// create material table.
			material_table.reserve(pAsManager_->GetTotalMaterialCount());
			pAsManager_->CreateMaterialTable(SetMaterialFunc);

			// create shader table.
			auto Align = [](UINT size, UINT align)
			{
				return ((size + align - 1) / align) * align;
			};
			UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
			UINT shaderRecordSize = Align(descHandleOffset + sizeof(LocalTable), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
			rtShaderRecordSize_ = shaderRecordSize;

			auto SetHitGroupFunc = [&](sl12::u8* pBuff, sl12::u32 index, void** ppHgId)
			{
				auto&& table = material_table[index];
				int hg_base_index = table.opaque ? 0 : 2;
				for (int id = 0; id < kRTMaterialTableCount; id++)
				{
					auto start = pBuff;

					memcpy(pBuff, ppHgId[hg_base_index + id], shaderIdentifierSize);
					pBuff += descHandleOffset;

					memcpy(pBuff, &table.table, sizeof(LocalTable));

					pBuff = start + shaderRecordSize;
				}
				return true;
			};
			auto CreateRGSorMSTable = [&](
				void* const* shaderIds,
				sl12::u32 tableCount,
				sl12::Buffer& buffer)
			{
				if (!buffer.Initialize(&device_, shaderRecordSize * tableCount, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
				{
					return false;
				}

				auto pBuff = reinterpret_cast<char*>(buffer.Map(nullptr));

				for (sl12::u32 id = 0; id < tableCount; id++)
				{
					auto start = pBuff;

					memcpy(pBuff, shaderIds[id], shaderIdentifierSize);
					pBuff += descHandleOffset;

					memcpy(pBuff, &material_table[0].table, sizeof(LocalTable));

					pBuff = start + shaderRecordSize;
				}

				buffer.Unmap();

				return true;
			};

			// for DirectShadow.
			{
				void* rgs_identifier;
				void* ms_identifier;
				void* hg_identifier[4];
				{
					ID3D12StateObjectProperties* prop;
					rtDirectShadowPSO_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
					rgs_identifier = prop->GetShaderIdentifier(kDirectShadowRGS);
					ms_identifier = prop->GetShaderIdentifier(kDirectShadowMS);
					hg_identifier[0] = hg_identifier[1] = prop->GetShaderIdentifier(kOcclusionOpacityHG);
					hg_identifier[2] = hg_identifier[3] = prop->GetShaderIdentifier(kOcclusionMaskedHG);
					prop->Release();
				}
				if (!CreateRGSorMSTable(&rgs_identifier, 1, rtDirectShadowRGSTable_))
				{
					return false;
				}
				if (!CreateRGSorMSTable(&ms_identifier, 1, rtDirectShadowMSTable_))
				{
					return false;
				}
				if (!pAsManager_->CreateHitGroupTable(
					shaderRecordSize,
					kRTMaterialTableCount,
					std::bind(SetHitGroupFunc, std::placeholders::_1, std::placeholders::_2, hg_identifier),
					rtDirectShadowHGTable_))
				{
					return false;
				}
			}
			// for RTReflection.
			{
				void* rgs_identifier;
				void* ms_identifier[2];
				void* hg_identifier[4];
				{
					ID3D12StateObjectProperties* prop;
					rtReflectionPSO_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
					rgs_identifier = prop->GetShaderIdentifier(kReflectionRGS);
					ms_identifier[0] = prop->GetShaderIdentifier(kReflectionMS);
					ms_identifier[1] = prop->GetShaderIdentifier(kDirectShadowMS);
					hg_identifier[0] = prop->GetShaderIdentifier(kMaterialOpacityHG);
					hg_identifier[1] = prop->GetShaderIdentifier(kOcclusionOpacityHG);
					hg_identifier[2] = prop->GetShaderIdentifier(kMaterialMaskedHG);
					hg_identifier[3] = prop->GetShaderIdentifier(kOcclusionMaskedHG);
					prop->Release();
				}
				if (!CreateRGSorMSTable(&rgs_identifier, 1, rtReflectionRGSTable_))
				{
					return false;
				}
				if (!CreateRGSorMSTable(ms_identifier, 2, rtReflectionMSTable_))
				{
					return false;
				}
				if (!pAsManager_->CreateHitGroupTable(
					shaderRecordSize,
					kRTMaterialTableCount,
					std::bind(SetHitGroupFunc, std::placeholders::_1, std::placeholders::_2, hg_identifier),
					rtReflectionHGTable_))
				{
					return false;
				}
			}
			// for ProbeLighting.
			{
				void* rgs_identifier;
				void* ms_identifier[2];
				void* hg_identifier[4];
				{
					ID3D12StateObjectProperties* prop;
					rtProbeLightingPSO_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
					rgs_identifier = prop->GetShaderIdentifier(kProbeLightingRGS);
					ms_identifier[0] = prop->GetShaderIdentifier(kMaterialMS);
					ms_identifier[1] = prop->GetShaderIdentifier(kDirectShadowMS);
					hg_identifier[0] = prop->GetShaderIdentifier(kMaterialOpacityHG);
					hg_identifier[1] = prop->GetShaderIdentifier(kOcclusionOpacityHG);
					hg_identifier[2] = prop->GetShaderIdentifier(kMaterialMaskedHG);
					hg_identifier[3] = prop->GetShaderIdentifier(kOcclusionMaskedHG);
					prop->Release();
				}
				if (!CreateRGSorMSTable(&rgs_identifier, 1, rtProbeLightingRGSTable_))
				{
					return false;
				}
				if (!CreateRGSorMSTable(ms_identifier, 2, rtProbeLightingMSTable_))
				{
					return false;
				}
				if (!pAsManager_->CreateHitGroupTable(
					shaderRecordSize,
					kRTMaterialTableCount,
					std::bind(SetHitGroupFunc, std::placeholders::_1, std::placeholders::_2, hg_identifier),
					rtProbeLightingHGTable_))
				{
					return false;
				}
			}
		}

		return true;
	}

	void DestroyRaytracing()
	{
		temporalBlendPso_.Destroy();
		temporalBlendRS_.Destroy();
		temporalBlendCS_.Destroy();

		rtProbeLightingRGSTable_.Destroy();
		rtProbeLightingMSTable_.Destroy();
		rtProbeLightingHGTable_.Destroy();
		rtReflectionRGSTable_.Destroy();
		rtReflectionMSTable_.Destroy();
		rtReflectionHGTable_.Destroy();
		rtDirectShadowRGSTable_.Destroy();
		rtDirectShadowMSTable_.Destroy();
		rtDirectShadowHGTable_.Destroy();

		rtGlobalRootSig_.Destroy();
		rtLocalRootSig_.Destroy();
		rtOcclusionCollection_.Destroy();
		rtMaterialCollection_.Destroy();
		rtDirectShadowPSO_.Destroy();
		rtReflectionPSO_.Destroy();
		rtProbeLightingPSO_.Destroy();

		sponzaInstance_.Destroy();
		sl12::SafeDelete(pRtDescManager_);
	}

	bool InitializeIrradianceMap(sl12::CommandList* pCmdList)
	{
		// irradiance map generation pipeline.
		{
			if (!irrGenCS_.Initialize(&device_, sl12::ShaderType::Compute, g_pIrrMapGenCS, sizeof(g_pIrrMapGenCS)))
			{
				return false;
			}

			if (!irrGenRS_.Initialize(&device_, &irrGenCS_))
			{
				return false;
			}

			sl12::ComputePipelineStateDesc desc;
			desc.pRootSignature = &irrGenRS_;
			desc.pCS = &irrGenCS_;

			if (!irrGenPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		auto hdr_tex = hSkyHdrRes_.GetItem<sl12::ResourceItemTexture>();
		auto&& base_desc = hdr_tex->GetTexture().GetTextureDesc();
		{
			sl12::TextureDesc desc{};
			desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			desc.width = base_desc.width >> 4;
			desc.height = base_desc.height >> 4;
			desc.depth = 1;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.mipLevels = 1;
			desc.sampleCount = 1;
			desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			desc.isRenderTarget = false;
			desc.isUav = true;

			if (!irrTex_.Initialize(&device_, desc))
			{
				return false;
			}
			if (!irrTexSrv_.Initialize(&device_, &irrTex_))
			{
				return false;
			}
			if (!irrTexUav_.Initialize(&device_, &irrTex_))
			{
				return false;
			}

			// generate compute.
			pCmdList->GetCommandList()->SetPipelineState(irrGenPso_.GetPSO());
			cullDescSet_.Reset();
			cullDescSet_.SetCsSrv(0, hdr_tex->GetTextureView().GetDescInfo().cpuHandle);
			cullDescSet_.SetCsSampler(0, hdrSampler_.GetDescInfo().cpuHandle);
			cullDescSet_.SetCsUav(0, irrTexUav_.GetDescInfo().cpuHandle);

			pCmdList->SetComputeRootSignatureAndDescriptorSet(&irrGenRS_, &cullDescSet_);
			pCmdList->GetCommandList()->Dispatch((desc.width + 7) / 8, (desc.height + 7) / 8, 1);

			pCmdList->TransitionBarrier(&irrTex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		return true;
	}

	void UpdateIrradianceMap(sl12::CommandList* pCmdList)
	{
		auto hdr_tex = hSkyHdrRes_.GetItem<sl12::ResourceItemTexture>();
		{
			auto&& desc = irrTex_.GetTextureDesc();

			pCmdList->TransitionBarrier(&irrTex_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// generate compute.
			pCmdList->GetCommandList()->SetPipelineState(irrGenPso_.GetPSO());
			cullDescSet_.Reset();
			cullDescSet_.SetCsSrv(0, hdr_tex->GetTextureView().GetDescInfo().cpuHandle);
			cullDescSet_.SetCsSampler(0, hdrSampler_.GetDescInfo().cpuHandle);
			cullDescSet_.SetCsUav(0, irrTexUav_.GetDescInfo().cpuHandle);

			pCmdList->SetComputeRootSignatureAndDescriptorSet(&irrGenRS_, &cullDescSet_);
			pCmdList->GetCommandList()->Dispatch((desc.width + 7) / 8, (desc.height + 7) / 8, 1);

			pCmdList->TransitionBarrier(&irrTex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		}
	}

	void DestroyIrradianceMap()
	{
		irrTex_.Destroy();
		irrTexSrv_.Destroy();
		irrTexUav_.Destroy();

		irrGenPso_.Destroy();
		irrGenRS_.Destroy();
		irrGenCS_.Destroy();
	}

	bool CreateSphere()
	{
		const int kLongitudeCount = 16;
		const int kLatitudeCount = 8;

		const int kVertexCount = kLongitudeCount * (kLatitudeCount - 1) + 2;
		if (!sphereVB_.Initialize(&device_, sizeof(DirectX::XMFLOAT3) * kVertexCount, sizeof(DirectX::XMFLOAT3), sl12::BufferUsage::VertexBuffer, true, false))
		{
			return false;
		}
		if (!sphereVBV_.Initialize(&device_, &sphereVB_))
		{
			return false;
		}
		{
			auto p = (DirectX::XMFLOAT3*)sphereVB_.Map(nullptr);
			*p = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f); p++;
			for (int y = 1; y < kLatitudeCount; y++)
			{
				float ay = DirectX::XM_PI * (float)y / (float)kLatitudeCount;
				float py = cos(ay);
				for (int x = 0; x < kLongitudeCount; x++)
				{
					float ax = DirectX::XM_2PI * (float)x / (float)kLongitudeCount;
					float px = cos(ax) * sin(ay);
					float pz = sin(ax) * sin(ay);
					*p = DirectX::XMFLOAT3(px, py, pz); p++;
				}
			}
			*p = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f); p++;
			sphereVB_.Unmap();
		}

		sphereIndexCount_ = (kLongitudeCount * 2 + kLongitudeCount * 2 * (kLatitudeCount - 2)) * 3;
		if (!sphereIB_.Initialize(&device_, sizeof(sl12::u32) * sphereIndexCount_, sizeof(sl12::u32), sl12::BufferUsage::IndexBuffer, true, false))
		{
			return false;
		}
		if (!sphereIBV_.Initialize(&device_, &sphereIB_))
		{
			return false;
		}
		{
			auto p = (sl12::u32*)sphereIB_.Map(nullptr);
			sl12::u32 startIndex = 0;
			for (int i = 0; i < kLongitudeCount; i++)
			{
				*p = startIndex; p++;
				*p = startIndex + i + 1; p++;
				*p = startIndex + ((i + 1) % kLongitudeCount) + 1; p++;
			}
			startIndex += 1;
			for (int y = 1; y < kLatitudeCount - 1; y++)
			{
				for (int x = 0; x < kLongitudeCount; x++)
				{
					*p = startIndex + x; p++;
					*p = startIndex + x + kLongitudeCount; p++;
					*p = startIndex + ((x + 1) % kLongitudeCount); p++;

					*p = startIndex + ((x + 1) % kLongitudeCount); p++;
					*p = startIndex + x + kLongitudeCount; p++;
					*p = startIndex + ((x + 1) % kLongitudeCount) + kLongitudeCount; p++;
				}
				startIndex += kLongitudeCount;
			}
			for (int i = 0; i < kLongitudeCount; i++)
			{
				*p = startIndex + i; p++;
				*p = startIndex + kLongitudeCount; p++;
				*p = startIndex + ((i + 1) % kLongitudeCount); p++;
			}
			sphereIB_.Unmap();
		}

		return true;
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
		static const int	kBufferCount = 3;
		sl12::Texture				gbufferTex[kBufferCount];
		sl12::TextureView			gbufferSRV[kBufferCount];
		sl12::RenderTargetView		gbufferRTV[kBufferCount];

		sl12::Texture				depthTex;
		sl12::TextureView			depthSRV;
		sl12::DepthStencilView		depthDSV;

		bool Initialize(sl12::Device* pDev, int width, int height)
		{
			{
				const DXGI_FORMAT kFormats[] = {
					DXGI_FORMAT_R10G10B10A2_UNORM,			// xyz:normal
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,		// xyz:baseColor
					DXGI_FORMAT_R16G16B16A16_FLOAT,			// xy:motionVector, z:roughness, w:metallic
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

	RaytracingResult			rtShadowResult_;
	RaytracingResult			rtReflectionResults_[2];
	int							rtCurrentReflectionResultIndex_ = -1;
	sl12::RootSignature			rtGlobalRootSig_, rtLocalRootSig_;
	sl12::DescriptorSet			rtGlobalDescSet_;
	sl12::DxrPipelineState		rtOcclusionCollection_;
	sl12::DxrPipelineState		rtMaterialCollection_;
	sl12::DxrPipelineState		rtDirectShadowPSO_;
	sl12::DxrPipelineState		rtReflectionPSO_;
	sl12::DxrPipelineState		rtProbeLightingPSO_;

	sl12::Shader				temporalBlendCS_;
	sl12::RootSignature			temporalBlendRS_;
	sl12::ComputePipelineState	temporalBlendPso_;

	sl12::Texture				irrTex_;
	sl12::TextureView			irrTexSrv_;
	sl12::UnorderedAccessView	irrTexUav_;
	sl12::Shader				irrGenCS_;
	sl12::RootSignature			irrGenRS_;
	sl12::ComputePipelineState	irrGenPso_;

	sl12::u32				rtShaderRecordSize_;
	sl12::Buffer			rtDirectShadowRGSTable_;
	sl12::Buffer			rtDirectShadowMSTable_;
	sl12::Buffer			rtDirectShadowHGTable_;
	sl12::Buffer			rtReflectionRGSTable_;
	sl12::Buffer			rtReflectionMSTable_;
	sl12::Buffer			rtReflectionHGTable_;
	sl12::Buffer			rtProbeLightingRGSTable_;
	sl12::Buffer			rtProbeLightingMSTable_;
	sl12::Buffer			rtProbeLightingHGTable_;

	sl12::Sampler			imageSampler_;
	sl12::Sampler			anisoSampler_;
	sl12::Sampler			hdrSampler_;
	sl12::Sampler			trilinearSampler_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer				lightCBs_[kBufferCount];
	sl12::ConstantBufferView	lightCBVs_[kBufferCount];

	sl12::Buffer				materialCBs_[kBufferCount];
	sl12::ConstantBufferView	materialCBVs_[kBufferCount];

	sl12::Buffer				glbMeshCB_;
	sl12::ConstantBufferView	glbMeshCBV_;

	// multi draw indirect関連
	sl12::Buffer					frustumCBs_[kBufferCount];
	sl12::ConstantBufferView		frustumCBVs_[kBufferCount];
	sl12::Shader					cullCS_, countClearCS_;
	sl12::RootSignature				cullRootSig_, countClearRootSig_;
	sl12::ComputePipelineState		cullPso_, countClearPso_;
	sl12::DescriptorSet				cullDescSet_;

	GBuffers				gbuffers_[kBufferCount];

	sl12::Shader				zpreVS_, zprePS_;
	sl12::Shader				lightingPS_;
	sl12::Shader				fullscreenVS_;
	sl12::RootSignature			zpreRootSig_, lightingRootSig_;
	sl12::GraphicsPipelineState	zprePso_, lightingPso_;
	ID3D12CommandSignature*		commandSig_ = nullptr;

	sl12::DescriptorSet			descSet_;

	sl12::Buffer				sphereVB_;
	sl12::VertexBufferView		sphereVBV_;
	sl12::Buffer				sphereIB_;
	sl12::IndexBufferView		sphereIBV_;
	sl12::u32					sphereIndexCount_;

	sl12::Shader				debugProbeVS_, debugProbePS_;
	sl12::RootSignature			debugProbeRS_;
	sl12::GraphicsPipelineState	debugProbePso_;

	sl12::Gui				gui_;
	sl12::InputData			inputData_{};

	sl12::Timestamp			gpuTimestamp_[sl12::Swapchain::kMaxBuffer];

	DirectX::XMFLOAT4		camPos_ = { -5.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		upVec_ = { 0.0f, 1.0f, 0.0f, 0.0f };
	float					skyPower_ = 0.1f;
	float					lightColor_[3] = { 1.0f, 1.0f, 1.0f };
	float					lightIntensity_ = 1.0f;
	DirectX::XMFLOAT3		spotLightPosition_ = { -3.0f, -1.0f, 0.0f };
	DirectX::XMVECTOR		spotLightRotation_ = DirectX::XMQuaternionIdentity();
	float					spotLightColor_[3] = { 0.8f, 0.4f, 0.1f };
	float					spotLightIntensity_ = 50.0f;
	float					spotLightRadius_ = 10.0f;
	float					spotLightOuterAngle_ = 30.0f;
	DirectX::XMFLOAT2		roughnessRange_ = { 0.0f, 1.0f };
	DirectX::XMFLOAT2		metallicRange_ = { 0.0f, 1.0f };
	uint32_t				loopCount_ = 0;
	bool					isClearTarget_ = true;
	float					giIntensity_ = 1.0f;
	float					reflectionIntensity_ = 0.0f;
	float					reflectionBlend_ = 0.02f;
	float					reflectionRoughnessMax_ = 0.4f;
	sl12::u32				reflectionFrameMax_ = 32;
	int						ddgiRelocationCount_ = 0;
	sl12::u32				frameTime_;

	DirectX::XMFLOAT4X4		mtxWorldToView_, mtxPrevWorldToView_;
	float					camRotX_ = 0.0f;
	float					camRotY_ = 0.0f;
	float					camMoveForward_ = 0.0f;
	float					camMoveLeft_ = 0.0f;
	float					camMoveUp_ = 0.0f;
	bool					isCameraMove_ = true;

	bool					isIndirectDraw_ = true;
	bool					isFreezeCull_ = false;
	bool					isStopSpot_ = false;
	bool					isDebugProbe_ = false;
	DirectX::XMFLOAT4X4		mtxFrustumViewProj_;

	int		frameIndex_ = 0;

	sl12::ResourceLoader	resLoader_;
	sl12::ResourceHandle	hMeshRes_[2];
	sl12::ResourceHandle	hSkyHdrRes_;
	sl12::ResourceHandle	hBlueNoiseRes_;
	int						sceneState_ = 0;		// 0:loading scene, 1:main scene

	sl12::MeshInstance		sponzaInstance_;
	sl12::MeshInstance		ballInstance_;
	sl12::ASManager*		pAsManager_ = nullptr;
	sl12::RaytracingDescriptorManager*	pRtDescManager_ = nullptr;
	sl12::ConstantBufferCache*	pCBCache_ = nullptr;

	std::unique_ptr<RtxgiComponent>	rtxgiComponent_;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
