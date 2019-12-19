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

#include "CompiledShaders/generate_sdf.lib.hlsl.h"
#include "CompiledShaders/clear_sdf.c.hlsl.h"
#include "CompiledShaders/fullscreen.vv.hlsl.h"
#include "CompiledShaders/ray_march.p.hlsl.h"

#include <windowsx.h>


namespace
{
	static const int	kScreenWidth = 1920;
	static const int	kScreenHeight = 1080;
	static const int	MaxSample = 512;
	static const float	kNearZ = 0.01f;
	static const float	kFarZ = 10000.0f;

	static const float	kSponzaScale = 5.0f;
	static const float	kSuzanneScale = 2.0f;
	static const float	kBoxScale = kSuzanneScale;

	static LPCWSTR		kRayGenName = L"RayGenerator";
	static LPCWSTR		kClosestHitName = L"ClosestHitProcessor";
	static LPCWSTR		kMissName = L"MissProcessor";
	static LPCWSTR		kHitGroupName = L"HitGroup";

	static const DXGI_FORMAT	kSdfFormat = DXGI_FORMAT_R16_FLOAT;
	static const sl12::u32		kSdfResolution = 64;
	static const sl12::u32		kRandomDirCount = 2048;
	static const float			kSdfBoxScale = 1.2f;

	class ConstBufferCache
	{
	public:
		struct ConstBuffer
		{
			sl12::Buffer				cb;
			sl12::ConstantBufferView	cbv;
			int							count = 2;

			ConstBuffer(sl12::Device* pDev, sl12::u32 size)
			{
				cb.Initialize(pDev, size, 0, sl12::BufferUsage::ConstantBuffer, true, false);
				cbv.Initialize(pDev, &cb);
			}

			~ConstBuffer()
			{
				cbv.Destroy();
				cb.Destroy();
			}
		};

		ConstBufferCache()
		{}
		~ConstBufferCache()
		{
			Destroy();
		}

		void Initialize(sl12::Device* pDevice)
		{
			pDevice_ = pDevice;
		}

		void Destroy()
		{
			for (auto&& lst : unused_)
			{
				for (auto&& b : lst.second)
					delete b;
			}
			for (auto&& lst : used_)
			{
				for (auto&& b : lst.second)
					delete b;
			}
			unused_.clear();
			used_.clear();
		}

		ConstBuffer* GetUnusedConstBuffer(sl12::u32 size, const void* pBuffer)
		{
			auto actual_size = ((size + 255) / 256) * 256;

			auto unused_lst = unused_.find(actual_size);
			if (unused_lst == unused_.end())
			{
				unused_[actual_size] = std::list<ConstBuffer*>();
				unused_lst = unused_.find(actual_size);
			}
			auto used_lst = used_.find(actual_size);
			if (used_lst == used_.end())
			{
				used_[actual_size] = std::list<ConstBuffer*>();
				used_lst = used_.find(actual_size);
			}

			ConstBuffer* ret = nullptr;
			if (!unused_lst->second.empty())
			{
				ret = *unused_lst->second.begin();
				unused_lst->second.erase(unused_lst->second.begin());
				used_lst->second.push_back(ret);
			}
			else
			{
				ret = new ConstBuffer(pDevice_, actual_size);
				used_lst->second.push_back(ret);
			}

			memcpy(ret->cb.Map(nullptr), pBuffer, size);
			ret->cb.Unmap();

			return ret;
		}

		void BeginNewFrame()
		{
			for (auto&& used_lst : used_)
			{
				auto&& unused_lst = unused_[used_lst.first];
				for (auto it = used_lst.second.begin(); it != used_lst.second.end();)
				{
					auto b = *it;
					b->count--;
					if (b->count == 0)
					{
						it = used_lst.second.erase(it);
						b->count = 2;
						unused_lst.push_back(b);
					}
					else
					{
						++it;
					}
				}
			}
		}

	private:
		sl12::Device*									pDevice_ = nullptr;
		std::map<sl12::u32, std::list<ConstBuffer*>>	unused_;
		std::map<sl12::u32, std::list<ConstBuffer*>>	used_;
	};	// class ConstBufferCache
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

	struct BoxInfo
	{
		DirectX::XMFLOAT3	centerPos;
		float				extentWhole;
		float				sizeVoxel;
		sl12::u32			resolution;
	};

	struct TraceInfo
	{
		sl12::u32			dirOffset;
		sl12::u32			dirCount;
	};

	struct MeshCB
	{
		DirectX::XMFLOAT4X4	mtxLocalToWorld;
		DirectX::XMFLOAT4X4	mtxPrevLocalToWorld;
	};

	struct BoxTransform
	{
		DirectX::XMFLOAT4X4	mtxLocalToWorld;
		DirectX::XMFLOAT4X4	mtxWorldToLocal;
	};

public:
	SampleApplication(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
		: Application(hInstance, nCmdShow, screenWidth, screenHeight)
	{}

	bool Initialize() override
	{
		resourceLoader_.Initialize(&device_);
		//hMeshRes_ = resourceLoader_.LoadRequest<sl12::ResourceItemMesh>("data/sponza/test.rmesh");
		hMeshRes_ = resourceLoader_.LoadRequest<sl12::ResourceItemMesh>("data/suzanne/suzanne.rmesh");

		// コマンドリストの初期化
		auto&& gqueue = device_.GetGraphicsQueue();
		auto&& cqueue = device_.GetComputeQueue();
		if (!mainCmdLists_.Initialize(&device_, &gqueue))
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
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			if (!imageSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}

		// ルートシグネチャの初期化
		if (!sl12::CreateRaytracingRootSignature(&device_,
			1,		// asCount
			2,		// globalCbvCount
			1,		// globalSrvCount
			1,		// globalUavCount
			0,		// globalSamplerCount
			&rtGlobalRootSig_,
			&rtLocalRootSig_))
		{
			return false;
		}
		if (!zpreRootSig_.Initialize(&device_, &zpreVS_, &zprePS_, nullptr, nullptr, nullptr))
		{
			return false;
		}
		if (!clearSdfRS_.Initialize(&device_, &clearSdfCS_))
		{
			return false;
		}
		if (!raymarchRS_.Initialize(&device_, &fullscreenVS_, &raymarchPS_, nullptr, nullptr, nullptr))
		{
			return false;
		}

		// パイプラインステートオブジェクトの初期化
		if (!CreatePipelineState())
		{
			return false;
		}
		{
			if (!clearSdfCS_.Initialize(&device_, sl12::ShaderType::Compute, g_pClearSdfCS, sizeof(g_pClearSdfCS)))
			{
				return false;
			}

			sl12::ComputePipelineStateDesc desc;
			desc.pCS = &clearSdfCS_;
			desc.pRootSignature = &clearSdfRS_;

			if (!clearSdfPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!fullscreenVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pFullscreenVS, sizeof(g_pFullscreenVS)))
			{
				return false;
			}
			if (!raymarchPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pRayMarchPS, sizeof(g_pRayMarchPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &raymarchRS_;
			desc.pVS = &fullscreenVS_;
			desc.pPS = &raymarchPS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = true;
			desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_SRC_ALPHA;
			desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_INV_SRC_ALPHA;
			desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ONE;
			desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ZERO;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			desc.inputLayout.numElements = 0;
			desc.inputLayout.pElements = nullptr;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = device_.GetSwapchain().GetCurrentTexture()->GetTextureDesc().format;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!raymarchPSO_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// ランダム方向ベクトルバッファを生成
		{
			if (!randomDirBuffer_.Initialize(&device_,
				sizeof(DirectX::XMFLOAT3) * kRandomDirCount,
				sizeof(DirectX::XMFLOAT3),
				sl12::BufferUsage::ShaderResource,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				true,
				false))
			{
				return false;
			}
			if (!randomDirBV_.Initialize(&device_, &randomDirBuffer_, 0, kRandomDirCount, sizeof(DirectX::XMFLOAT3)))
			{
				return false;
			}

			auto RadicalInverseVdC = [](sl12::u32 bits)
			{
				bits = (bits << 16u) | (bits >> 16u);
				bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
				bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
				bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
				bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
				return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
			};
			auto Hammersley2D = [&](sl12::u32 i, sl12::u32 N)
			{
				DirectX::XMFLOAT2 ret;
				ret.x = float(i) / float(N);
				ret.y = RadicalInverseVdC(i);
				return ret;
			};
			auto HemisphereSampleUniform = [&](float u, float v)
			{
				float phi = v * 2.0f * 3.1415926f;
				float cosTheta = 1.0f - u;
				float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);
				return DirectX::XMFLOAT3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
			};
			std::array<DirectX::XMFLOAT3, kRandomDirCount> data;
			for (sl12::u32 i = 0; i < kRandomDirCount / 2; i++)
			{
				auto ham = Hammersley2D(i, kRandomDirCount / 2);
				data[i] = HemisphereSampleUniform(ham.x, ham.y);
			}
			for (sl12::u32 i = 0; i < kRandomDirCount / 2; i++)
			{
				auto v = data[i];
				v.z = -v.z;
				data[i + (kRandomDirCount / 2)] = v;
			}
			auto p = randomDirBuffer_.Map(nullptr);
			memcpy(p, data.data(), sizeof(DirectX::XMFLOAT3) * kRandomDirCount);
			randomDirBuffer_.Unmap();
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

		// 定数バッファを生成する
		if (!CreateSceneCB())
		{
			return false;
		}
		if (!CreateMeshCB())
		{
			return false;
		}

		// ボックス用トランスフォーム情報を生成
		{
			if (!boxTransCB_.Initialize(&device_, sizeof(BoxTransform), 0, sl12::BufferUsage::ConstantBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}
			if (!boxTransCBV_.Initialize(&device_, &boxTransCB_))
			{
				return false;
			}

			DirectX::XMMATRIX ml2w = DirectX::XMMatrixScaling(kBoxScale, kBoxScale, kBoxScale);
			DirectX::XMMATRIX mw2l = DirectX::XMMatrixInverse(nullptr, ml2w);

			auto p = (BoxTransform*)boxTransCB_.Map(nullptr);
			DirectX::XMStoreFloat4x4(&p->mtxLocalToWorld, ml2w);
			DirectX::XMStoreFloat4x4(&p->mtxWorldToLocal, mw2l);
			boxTransCB_.Unmap();
		}

		constBufferCache_.Initialize(&device_);

		return true;
	}

	bool Execute() override
	{
		const int kSwapchainBufferOffset = 1;
		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto prevFrameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 2) % sl12::Swapchain::kMaxBuffer;
		auto&& currCmdList = mainCmdLists_.Reset();
		auto pCmdList = &currCmdList;
		auto d3dCmdList = pCmdList->GetCommandList();
		auto dxrCmdList = pCmdList->GetDxrCommandList();
		auto&& swapchain = device_.GetSwapchain();

		// 共通の描画開始時処理
		device_.WaitPresent();
		device_.SyncKillObjects();

		constBufferCache_.BeginNewFrame();
		gui_.BeginNewFrame(&currCmdList, kScreenWidth, kScreenHeight, inputData_);

		device_.LoadRenderCommands(pCmdList);

		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle, color, 0, nullptr);
		}

		// ステートに合わせた処理
		if (appState_ == STATE_MESH_LOAD)
		{
			static int sLoadingTime = 0;
			std::string t = "Resource loading.";
			for (int i = 0; i < sLoadingTime; i++)
			{
				t += '.';
			}
			ImGui::Text(t.c_str());
			sLoadingTime = (sLoadingTime + 1) % 3;

			// レンダーターゲット設定
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

			if (!resourceLoader_.IsLoading())
			{
				appState_ = STATE_INIT_SDF;
			}
		}
		else if (appState_ == STATE_INIT_SDF)
		{
			if (pSdfTexture_)
			{
				device_.KillObject(pSdfTexture_);
				device_.KillObject(pSdfTextureSRV_);
				device_.KillObject(pSdfTextureUAV_);
			}
			{
				pSdfTexture_ = new sl12::Texture();
				pSdfTextureSRV_ = new sl12::TextureView();
				pSdfTextureUAV_ = new sl12::UnorderedAccessView();

				sl12::TextureDesc desc;
				desc.dimension = sl12::TextureDimension::Texture3D;
				desc.width = desc.height = desc.depth = sdfResolution_;
				desc.mipLevels = 1;
				desc.format = kSdfFormat;
				desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				desc.sampleCount = 1;
				desc.clearColor[0] = desc.clearColor[1] = desc.clearColor[2] = desc.clearColor[3] = 0.0f;
				desc.clearDepth = 1.0f;
				desc.clearStencil = 0;
				desc.isRenderTarget = false;
				desc.isDepthBuffer = false;
				desc.isUav = true;
				if (!pSdfTexture_->Initialize(&device_, desc))
				{
					return false;
				}

				if (!pSdfTextureSRV_->Initialize(&device_, pSdfTexture_))
				{
					return false;
				}

				if (!pSdfTextureUAV_->Initialize(&device_, pSdfTexture_, 0, 0, sdfResolution_))
				{
					return false;
				}
			}

			auto pMesh = hMeshRes_.GetItem<sl12::ResourceItemMesh>();

			// Raytracing用DescriptorHeapの初期化
			if (!rtDescMan_.Initialize(&device_,
				1,		// renderCount
				1,		// asCount
				2,		// globalCbvCount
				1,		// globalSrvCount
				1,		// globalUavCount
				0,		// globalSamplerCount
				(sl12::u32)pMesh->GetSubmeshes().size()))
			{
				return false;
			}

			// ASを生成する
			if (!CreateAccelerationStructure(pCmdList))
			{
				return false;
			}

			// シェーダテーブルを生成する
			if (!CreateShaderTable())
			{
				return false;
			}

			// SDFを生成するバウンディングボックスの情報を生成
			if (pBoxInfoCB_)
			{
				device_.KillObject(pBoxInfoCBV_);
				device_.KillObject(pBoxInfoCB_);
			}
			{
				pBoxInfoCB_ = new sl12::Buffer();
				pBoxInfoCBV_ = new sl12::ConstantBufferView();

				if (!pBoxInfoCB_->Initialize(&device_, sizeof(BoxInfo), 0, sl12::BufferUsage::ConstantBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
				{
					return false;
				}
				if (!pBoxInfoCBV_->Initialize(&device_, pBoxInfoCB_))
				{
					return false;
				}

				auto&& bb = pMesh->GetBoundingInfo();
				float cx = (bb.box.aabbMax.x + bb.box.aabbMin.x) * 0.5f;
				float cy = (bb.box.aabbMax.y + bb.box.aabbMin.y) * 0.5f;
				float cz = (bb.box.aabbMax.z + bb.box.aabbMin.z) * 0.5f;
				float ex = (bb.box.aabbMax.x - bb.box.aabbMin.x) * 0.5f;
				float ey = (bb.box.aabbMax.y - bb.box.aabbMin.y) * 0.5f;
				float ez = (bb.box.aabbMax.z - bb.box.aabbMin.z) * 0.5f;
				float extent = std::max(ex, std::max(ey, ez)) * kSdfBoxScale;

				auto p = (BoxInfo*)pBoxInfoCB_->Map(nullptr);
				p->centerPos = DirectX::XMFLOAT3(cx, cy, cz);
				p->extentWhole = extent;
				p->sizeVoxel = extent * 2.0f / (float)sdfResolution_;
				p->resolution = sdfResolution_;
				pBoxInfoCB_->Unmap();
			}

			// SDFテクスチャを初期化する
			d3dCmdList->SetPipelineState(clearSdfPSO_.GetPSO());
			clearSdfDescSet_.Reset();
			clearSdfDescSet_.SetCsUav(0, pSdfTextureUAV_->GetDescInfo().cpuHandle);
			pCmdList->SetComputeRootSignatureAndDescriptorSet(&clearSdfRS_, &clearSdfDescSet_);
			d3dCmdList->Dispatch(sdfResolution_, sdfResolution_, sdfResolution_);

			appState_ = STATE_CREATE_SDF;
		}
		else if (appState_ == STATE_CREATE_SDF)
		{
			static sl12::u32 sTraceCount = 0;
			static const sl12::u32 kTraceCountPerFrame = 32;

			ImGui::Text("Creating sdf : %d to %d", sTraceCount, sTraceCount + kTraceCountPerFrame);

			// レイトレース用の定数バッファを生成
			auto pTraceCB = new sl12::Buffer();
			auto pTraceCBV = new sl12::ConstantBufferView();
			{
				if (!pTraceCB->Initialize(&device_, sizeof(TraceInfo), 0, sl12::BufferUsage::ConstantBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
				{
					return false;
				}
				if (!pTraceCBV->Initialize(&device_, pTraceCB))
				{
					return false;
				}

				auto p = (TraceInfo*)pTraceCB->Map(nullptr);
				p->dirOffset = sTraceCount;
				p->dirCount = kTraceCountPerFrame;
				pTraceCB->Unmap();
			}

			// デスクリプタを設定
			rtGlobalDescSet_.Reset();
			rtGlobalDescSet_.SetCsCbv(0, pBoxInfoCBV_->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsCbv(1, pTraceCBV->GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(1, randomDirBV_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(0, pSdfTextureUAV_->GetDescInfo().cpuHandle);

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
			desc.Width = desc.Height = desc.Depth = sdfResolution_;
			dxrCmdList->SetPipelineState1(stateObject_.GetPSO());
			dxrCmdList->DispatchRays(&desc);

			// レイトレース終了処理
			device_.KillObject(pTraceCBV);
			device_.KillObject(pTraceCB);

			sTraceCount += kTraceCountPerFrame;
			if (sTraceCount >= kRandomDirCount)
			{
				sTraceCount = 0;
				pCmdList->TransitionBarrier(pSdfTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				appState_ = STATE_DRAW;
			}

			// レンダーターゲット設定
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
		else if (appState_ == STATE_DRAW)
		{
			UpdateSceneCB(frameIndex);
			UpdateMoveBox();
			ControlCamera();
			if (isCameraMove_)
			{
				isCameraMove_ = false;
			}

			{
				const char* kResoItems[] = {
					"16",
					"32",
					"64",
					"128",
					"256",
				};
				int reso_item = 0;
				for (; reso_item < ARRAYSIZE(kResoItems); reso_item++)
				{
					if (nextSdfResolution_ == atoi(kResoItems[reso_item]))
					{
						break;
					}
				}
				if (ImGui::Combo("SDF Reso", &reso_item, kResoItems, ARRAYSIZE(kResoItems)))
				{
					nextSdfResolution_ = atoi(kResoItems[reso_item]);
				}
				if (ImGui::Button("Apply"))
				{
					if (sdfResolution_ != nextSdfResolution_)
					{
						sdfResolution_ = nextSdfResolution_;
						appState_ = STATE_INIT_SDF;
					}
				}
			}

			DirectX::XMMATRIX mtxMove = DirectX::XMMatrixScaling(kBoxScale, kBoxScale, kBoxScale) * DirectX::XMMatrixRotationY(moveYAngle_)  * DirectX::XMMatrixRotationX(moveXAngle_) * DirectX::XMMatrixTranslation(movePos_.x, movePos_.y, movePos_.z);
			DirectX::XMMATRIX mtxInvMove = DirectX::XMMatrixInverse(nullptr, mtxMove);
			BoxTransform moveTrans;
			DirectX::XMStoreFloat4x4(&moveTrans.mtxLocalToWorld, mtxMove);
			DirectX::XMStoreFloat4x4(&moveTrans.mtxWorldToLocal, mtxInvMove);
			auto moveCB = constBufferCache_.GetUnusedConstBuffer(sizeof(BoxTransform), &moveTrans);

			// レンダーターゲット設定
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

			// PSO設定
			d3dCmdList->SetPipelineState(raymarchPSO_.GetPSO());

			// 基本Descriptor設定
			raymarchDescSet_.Reset();
			raymarchDescSet_.SetPsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			raymarchDescSet_.SetPsCbv(1, pBoxInfoCBV_->GetDescInfo().cpuHandle);
			raymarchDescSet_.SetPsCbv(2, boxTransCBV_.GetDescInfo().cpuHandle);
			raymarchDescSet_.SetPsCbv(3, moveCB->cbv.GetDescInfo().cpuHandle);
			raymarchDescSet_.SetPsSrv(0, pSdfTextureSRV_->GetDescInfo().cpuHandle);
			raymarchDescSet_.SetPsSampler(0, imageSampler_.GetDescInfo().cpuHandle);
			pCmdList->SetGraphicsRootSignatureAndDescriptorSet(&raymarchRS_, &raymarchDescSet_);
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3dCmdList->DrawInstanced(3, 1, 0, 0);
		}

		// 共通の描画終了時処理
		ImGui::Render();
		
		pCmdList->TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		// コマンド終了と描画待ち
		mainCmdLists_.Close();
		device_.WaitDrawDone();

		// 次のフレームへ
		device_.Present(1);

		// コマンド実行
		mainCmdLists_.Execute();

		return true;
	}

	void Finalize() override
	{
		// 描画待ち
		device_.WaitDrawDone();
		device_.Present(0);

		if (pSdfTextureSRV_) delete pSdfTextureSRV_;
		if (pSdfTextureUAV_) delete pSdfTextureUAV_;
		if (pSdfTexture_) delete pSdfTexture_;
		if (pBoxInfoCBV_) delete pBoxInfoCBV_;
		if (pBoxInfoCB_) delete pBoxInfoCB_;

		constBufferCache_.Destroy();

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
		bottomAS_.Destroy();

		for (auto&& v : gbuffers_) v.Destroy();

		fullscreenVS_.Destroy();
		raymarchPS_.Destroy();
		raymarchPSO_.Destroy();
		raymarchRS_.Destroy();

		randomDirBV_.Destroy();
		randomDirBuffer_.Destroy();
		clearSdfRS_.Destroy();
		clearSdfPSO_.Destroy();

		denoiseXPso_.Destroy();
		denoiseYPso_.Destroy();
		denoiseXPS_.Destroy();
		denoiseYPS_.Destroy();
		temporalPso_.Destroy();
		temporalPS_.Destroy();
		lightingPso_.Destroy();
		lightingVS_.Destroy();
		lightingPS_.Destroy();
		zprePso_.Destroy();
		zpreVS_.Destroy();
		zprePS_.Destroy();
		stateObject_.Destroy();

		rtDescMan_.Destroy();
		temporalRootSig_.Destroy();
		lightingRootSig_.Destroy();
		zpreRootSig_.Destroy();
		rtLocalRootSig_.Destroy();
		rtGlobalRootSig_.Destroy();

		asFence_.Destroy();
		utilCmdList_.Destroy();
		mainCmdLists_.Destroy();
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
			{ kClosestHitName,	nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kMissName,		nullptr, D3D12_EXPORT_FLAG_NONE },
		};
		dxrDesc.AddDxilLibrary(g_pGenerateSdfLib, sizeof(g_pGenerateSdfLib), libExport, ARRAYSIZE(libExport));

		// ヒットグループサブオブジェクト
		// Intersection, AnyHit, ClosestHitの組み合わせを定義し、ヒットグループ名でまとめるサブオブジェクトです.
		// マテリアルごとや用途ごと(マテリアル、シャドウなど)にサブオブジェクトを用意します.
		dxrDesc.AddHitGroup(kHitGroupName, true, nullptr, kClosestHitName, nullptr);

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
			kHitGroupName,
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

	bool CreateBottomASFromGlbMesh(sl12::CommandList* pCmdList, sl12::ResourceItemMesh* pMesh, sl12::BottomAccelerationStructure* pBottomAS)
	{
		// Bottom ASの生成準備
		auto&& submeshes = pMesh->GetSubmeshes();
		std::vector<sl12::GeometryStructureDesc> geoDescs(submeshes.size());
		for (int i = 0; i < submeshes.size(); i++)
		{
			auto&& submesh = submeshes[i];
			auto&& material = pMesh->GetMaterials()[submesh.materialIndex];

			auto flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
			geoDescs[i].InitializeAsTriangle(
				flags,
				&pMesh->GetPositionVB(),
				&pMesh->GetIndexBuffer(),
				nullptr,
				submesh.positionVBV.GetView().StrideInBytes,
				(UINT)(submesh.positionVBV.GetView().SizeInBytes / submesh.positionVBV.GetView().StrideInBytes),
				(UINT64)(submesh.positionVBV.GetBufferOffset()),
				DXGI_FORMAT_R32G32B32_FLOAT,
				(UINT)(submesh.indexBV.GetView().SizeInBytes / (submesh.indexBV.GetView().Format == DXGI_FORMAT_R32_UINT ? 4 : 2)),
				(UINT64)(submesh.indexBV.GetBufferOffset()),
				submesh.indexBV.GetView().Format);
		}

		sl12::StructureInputDesc bottomInput{};
		if (!bottomInput.InitializeAsBottom(&device_, geoDescs.data(), (UINT)geoDescs.size(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
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

	bool CreateAccelerationStructure(sl12::CommandList* cmdList)
	{
		// ASの生成はGPUで行うため、コマンドを積みGPUを動作させる必要があります.
		// BottomASの生成
		auto pMesh = hMeshRes_.GetItem<sl12::ResourceItemMesh>();
		if (!CreateBottomASFromGlbMesh(cmdList, const_cast<sl12::ResourceItemMesh*>(pMesh), &bottomAS_))
		{
			return false;
		}

		// Top ASの生成準備
		std::array<sl12::TopInstanceDesc, 1> topInstances;
		DirectX::XMMATRIX mtx_unit = DirectX::XMMatrixIdentity();
		DirectX::XMFLOAT4X4 mtx;
		DirectX::XMStoreFloat4x4(&mtx, mtx_unit);
		topInstances[0].Initialize(mtx, 0, 0xff, 0, 0, &bottomAS_);

		if (!CreateTopAS(cmdList, topInstances.data(), (int)topInstances.size(), &topAS_))
		{
			return false;
		}

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
		auto pMesh = hMeshRes_.GetItem<sl12::ResourceItemMesh>();

		// LocalRS用のマテリアルDescriptorを用意する
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpu_handles;
		std::vector<int> hitGroupIndices;
		auto view_desc_size = rtDescMan_.GetViewDescSize();
		auto sampler_desc_size = rtDescMan_.GetSamplerDescSize();
		auto local_handle_start = rtDescMan_.IncrementLocalHandleStart();

		for (int i = 0; i < pMesh->GetSubmeshes().size(); i++)
		{
			hitGroupIndices.push_back(0);

			// CBVなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);

			// SRVなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);

			// UAVなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);

			// Samplerなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);
		}

		// レイ生成シェーダ、ミスシェーダ、ヒットグループのIDを取得します.
		// 各シェーダ種別ごとにシェーダテーブルを作成しますが、このサンプルでは各シェーダ種別はそれぞれ1つのシェーダを持つことになります.
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier[1];
		{
			ID3D12StateObjectProperties* prop;
			stateObject_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
			rayGenShaderIdentifier = prop->GetShaderIdentifier(kRayGenName);
			missShaderIdentifier = prop->GetShaderIdentifier(kMissName);
			hitGroupShaderIdentifier[0] = prop->GetShaderIdentifier(kHitGroupName);
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
		if (!GenShaderTable(hitGroupShaderIdentifier, 1, hitGroupTable_, (int)pMesh->GetSubmeshes().size(), hitGroupIndices.data()))
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

	void UpdateMoveBox()
	{
		static float	sAngle = 0.0f;

		moveYAngle_ = DirectX::XMConvertToRadians(sAngle);
		moveXAngle_ = DirectX::XMConvertToRadians(sAngle * 1.33f);
		float sa = sinf(moveYAngle_);
		movePos_ = DirectX::XMFLOAT3(kBoxScale * 0.5f, 0.2f + sa * kBoxScale, kBoxScale * 0.2f);

		sAngle += 1.0f;
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
	CommandLists			mainCmdLists_;
	sl12::CommandList		utilCmdList_;
	sl12::Fence				asFence_;

	sl12::Texture*				pSdfTexture_ = nullptr;
	sl12::TextureView*			pSdfTextureSRV_ = nullptr;
	sl12::UnorderedAccessView*	pSdfTextureUAV_ = nullptr;

	sl12::RootSignature		rtGlobalRootSig_, rtLocalRootSig_;
	sl12::RaytracingDescriptorManager	rtDescMan_;
	sl12::DescriptorSet		rtGlobalDescSet_;

	sl12::DxrPipelineState		stateObject_;

	sl12::Sampler			imageSampler_;

	sl12::BottomAccelerationStructure	bottomAS_;
	sl12::TopAccelerationStructure		topAS_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer				glbMeshCB_;
	sl12::ConstantBufferView	glbMeshCBV_;

	sl12::Buffer			rayGenTable_, missTable_, hitGroupTable_;
	sl12::u64				shaderRecordSize_;

	GBuffers				gbuffers_[kBufferCount];

	sl12::Shader				zpreVS_, zprePS_;
	sl12::Shader				lightingVS_, lightingPS_;
	sl12::Shader				temporalPS_, denoiseXPS_, denoiseYPS_;
	sl12::RootSignature			zpreRootSig_, lightingRootSig_, temporalRootSig_;
	sl12::GraphicsPipelineState	zprePso_, lightingPso_, temporalPso_, denoiseXPso_, denoiseYPso_;
	ID3D12CommandSignature*		commandSig_ = nullptr;

	sl12::Buffer				randomDirBuffer_;
	sl12::BufferView			randomDirBV_;

	sl12::Shader				clearSdfCS_;
	sl12::RootSignature			clearSdfRS_;
	sl12::ComputePipelineState	clearSdfPSO_;
	sl12::DescriptorSet			clearSdfDescSet_;

	sl12::Shader				fullscreenVS_, raymarchPS_;
	sl12::RootSignature			raymarchRS_;
	sl12::GraphicsPipelineState	raymarchPSO_;
	sl12::DescriptorSet			raymarchDescSet_;

	sl12::Buffer*				pBoxInfoCB_;
	sl12::ConstantBufferView*	pBoxInfoCBV_;

	sl12::Buffer				boxTransCB_;
	sl12::ConstantBufferView	boxTransCBV_;

	sl12::DescriptorSet			descSet_;

	sl12::Gui				gui_;
	sl12::InputData			inputData_{};

	sl12::Timestamp			gpuTimestamp_[sl12::Swapchain::kMaxBuffer];
	sl12::Timestamp			bottomAsTimestamp_;
	sl12::Timestamp			topAsTimestamp_[sl12::Swapchain::kMaxBuffer];

	DirectX::XMFLOAT4		camPos_ = { 0.0f, 0.0f, 10.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, 0.0f, 0.0f, 1.0f };
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

	int						sdfResolution_ = kSdfResolution;
	int						nextSdfResolution_ = kSdfResolution;

	DirectX::XMFLOAT3		movePos_;
	float					moveYAngle_;
	float					moveXAngle_;

	bool					isIndirectDraw_ = true;
	bool					isFreezeCull_ = false;
	DirectX::XMFLOAT4X4		mtxFrustumViewProj_;

	int		frameIndex_ = 0;

	sl12::ResourceLoader	resourceLoader_;
	sl12::ResourceHandle	hMeshRes_;
	ConstBufferCache		constBufferCache_;

	enum AppState
	{
		STATE_MESH_LOAD,
		STATE_INIT_SDF,
		STATE_CREATE_SDF,
		STATE_DRAW,
	};
	AppState				appState_ = STATE_MESH_LOAD;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
