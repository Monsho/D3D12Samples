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

#include "CompiledShaders/test.lib.hlsl.h"
#include "CompiledShaders/copy.vv.hlsl.h"
#include "CompiledShaders/copy.p.hlsl.h"

#include <windowsx.h>

//#define USE_MEET_MAT


namespace
{
	static const int	kScreenWidth = 1280;
	static const int	kScreenHeight = 720;
	static const int	MaxSample = 2048;

	static LPCWSTR		kRayGenName = L"RayGenerator";
	static LPCWSTR		kClosestHitName = L"ClosestHitProcessor";
	static LPCWSTR		kClosestHitShadowName = L"ClosestHitShadowProcessor";
	static LPCWSTR		kAnyHitName = L"AnyHitProcessor";
	static LPCWSTR		kMissName = L"MissProcessor";
	static LPCWSTR		kMissShadowName = L"MissShadowProcessor";
	static LPCWSTR		kHitGroupName = L"HitGroup";
	static LPCWSTR		kHitGroupShadowName = L"HitGroupShadow";
}

class SampleApplication
	: public sl12::Application
{
	struct SceneCB
	{
		DirectX::XMFLOAT4X4	mtxProjToWorld;
		DirectX::XMFLOAT4	camPos;
		DirectX::XMFLOAT4	lightDir;
		DirectX::XMFLOAT4	lightColor;
		float				skyPower;
		uint32_t			loopCount;
		uint32_t			maxBounces;
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
		for (auto&& v : cmdLists_)
		{
			if (!v.Initialize(&device_, &gqueue, true))
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
		if (!sl12::CreateRaytracingRootSignature(&device_, 1, 1, 1, 2, 0, &rtGlobalRootSig_, &rtLocalRootSig_))
		{
			return false;
		}
		if (!copyRootSig_.Initialize(&device_, &copyVS_, &copyPS_, nullptr, nullptr, nullptr))
		{
			return false;
		}

		// パイプラインステートオブジェクトの初期化
		if (!CreatePipelineState())
		{
			return false;
		}
		{
			if (!copyVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pCopyVS, sizeof(g_pCopyVS)))
			{
				return false;
			}
			if (!copyPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pCopyPS, sizeof(g_pCopyPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &copyRootSig_;
			desc.pVS = &copyVS_;
			desc.pPS = &copyPS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = false;
			desc.rasterizer.isFrontCCW = true;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			desc.inputLayout.numElements = 0;
			desc.inputLayout.pElements = nullptr;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!copyPso_.Initialize(&device_, desc))
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
			desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			//desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			desc.sampleCount = 1;
			desc.clearColor[4] = { 0.0f };
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
		}

		// 乱数用のバッファを生成
		{
			if (!randomBuffer_.Initialize(&device_, sizeof(float) * 65536, sizeof(float), sl12::BufferUsage::ShaderResource, true, false))
			{
				return false;
			}
			if (!randomBufferSRV_.Initialize(&device_, &randomBuffer_, 0, sizeof(float)))
			{
				return false;
			}
			if (!seedBuffer_.Initialize(&device_, sizeof(uint32_t), 1, sl12::BufferUsage::ShaderResource, false, true))
			{
				return false;
			}
			if (!seedBufferUAV_.Initialize(&device_, &seedBuffer_))
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

		// GUIの初期化
		if (!gui_.Initialize(&device_, DXGI_FORMAT_R8G8B8A8_UNORM))
		{
			return false;
		}
		if (!gui_.CreateFontImage(&device_, cmdLists_[0]))
		{
			return false;
		}

		// ジオメトリを生成する
		if (!CreateGeometry())
		{
			return false;
		}

		// Raytracing用DescriptorHeapの初期化
		if (!rtDescMan_.Initialize(&device_, 1, 1, 1, 1, 2, 0, glbMesh_.GetSubmeshCount()))
		{
			return false;
		}

		// ASを生成する
		if (!CreateAccelerationStructure())
		{
			return false;
		}

		// シーン定数バッファを生成する
		if (!CreateSceneCB())
		{
			return false;
		}

		// シェーダテーブルを生成する
		if (!CreateShaderTable())
		{
			return false;
		}

		// タイムスタンプクエリとバッファ
		for (int i = 0; i < ARRAYSIZE(gpuTimestamp_); ++i)
		{
			if (!gpuTimestamp_[i].Initialize(&device_, 5))
			{
				return false;
			}
		}

		return true;
	}

	bool Execute() override
	{
		device_.WaitPresent();

		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto prevFrameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 2) % sl12::Swapchain::kMaxBuffer;
		auto&& cmdList = cmdLists_[frameIndex];
		auto&& d3dCmdList = cmdList.GetCommandList();
		auto&& dxrCmdList = cmdList.GetDxrCommandList();

		UpdateSceneCB(frameIndex);

		cmdList.Reset();

		gui_.BeginNewFrame(&cmdList, kScreenWidth, kScreenHeight, inputData_);

		// GUI
		{
			if (ImGui::SliderAngle("Camera Angle", &camRotAngle_))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::SliderFloat("Sky Power", &skyPower_, 0.0f, 100.0f))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::SliderFloat("Light Intensity", &lightPower_, 0.0f, 10.0f))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::ColorEdit3("Light Color", lightColor_))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}
			if (ImGui::SliderInt("Max Bounces", &maxBounces_, 1, 8))
			{
				isClearTarget_ = true;
				loopCount_ = 0;
			}

			uint64_t timestamp[5];
			gpuTimestamp_[prevFrameIndex].GetTimestamp(0, 5, timestamp);
			uint64_t all_time = timestamp[3] - timestamp[0];
			uint64_t ray_time = timestamp[2] - timestamp[1];
			uint64_t freq = device_.GetGraphicsQueue().GetTimestampFrequency();
			float all_ms = (float)all_time / ((float)freq / 1000.0f);
			float ray_ms = (float)ray_time / ((float)freq / 1000.0f);

			ImGui::Text("All GPU: %f (ms)", all_ms);
			ImGui::Text("RayTracing: %f (ms)", ray_ms);
		}

		gpuTimestamp_[frameIndex].Reset();
		gpuTimestamp_[frameIndex].Query(&cmdList);

		auto&& swapchain = device_.GetSwapchain();
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView()->GetDescInfo().cpuHandle, color, 0, nullptr);
		}

		if (isClearTarget_)
		{
			isClearTarget_ = false;
			float color[4] = { 0.0f };
			cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
			d3dCmdList->ClearRenderTargetView(resultTextureRTV_.GetDescInfo().cpuHandle, color, 0, nullptr);
			cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		// デスクリプタヒープを設定
		//ID3D12DescriptorHeap* pDescHeaps[] = {
		//	device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
		//	device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
		//};
		//d3dCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescHeaps), pDescHeaps);

		gpuTimestamp_[frameIndex].Query(&cmdList);

		if (loopCount_ < MaxSample)
		{
			// デスクリプタを設定
			rtGlobalDescSet_.Reset();
			rtGlobalDescSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsSrv(1, randomBufferSRV_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(0, resultTextureUAV_.GetDescInfo().cpuHandle);
			rtGlobalDescSet_.SetCsUav(1, seedBufferUAV_.GetDescInfo().cpuHandle);

			// コピーしつつコマンドリストに積む
			D3D12_GPU_VIRTUAL_ADDRESS as_address[] = {
				topAS_.GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress(),
			};
			cmdList.SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, &rtDescMan_, as_address, ARRAYSIZE(as_address));

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

			cmdList.UAVBarrier(&resultTexture_);
		}

		gpuTimestamp_[frameIndex].Query(&cmdList);

		// リソースバリア
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		{
			// レンダーターゲット設定
			auto&& rtv = swapchain.GetCurrentRenderTargetView()->GetDescInfo().cpuHandle;
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
			d3dCmdList->SetPipelineState(copyPso_.GetPSO());

			// RootSignatureとDescriptor設定
			descSet_.Reset();
			descSet_.SetPsSrv(0, resultTextureSRV_.GetDescInfo().cpuHandle);
			descSet_.SetPsSampler(0, linearClampSampler_.GetDescInfo().cpuHandle);
			cmdList.SetGraphicsRootSignatureAndDescriptorSet(&copyRootSig_, &descSet_);

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3dCmdList->IASetVertexBuffers(0, 0, nullptr);
			d3dCmdList->IASetIndexBuffer(nullptr);
			d3dCmdList->DrawInstanced(3, 1, 0, 0);
		}

		ImGui::Render();

		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		gpuTimestamp_[frameIndex].Query(&cmdList);
		gpuTimestamp_[frameIndex].Resolve(&cmdList);

		// コマンド終了と実行
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		// 次のフレームへ
		device_.Present(1);

		return true;
	}

	void Finalize() override
	{
		rayGenTable_.Destroy();
		missTable_.Destroy();
		hitGroupTable_.Destroy();

		for (auto&& v : sceneCBVs_) v.Destroy();
		for (auto&& v : sceneCBs_) v.Destroy();

		for (auto&& v : gpuTimestamp_) v.Destroy();

		gui_.Destroy();

		topAS_.Destroy();
		bottomAS_.Destroy();

		instanceSBV_.Destroy();
		instanceSB_.Destroy();
		spheresAABB_.Destroy();

		randomBufferSRV_.Destroy();
		randomBuffer_.Destroy();
		seedBufferUAV_.Destroy();
		seedBuffer_.Destroy();

		resultTextureUAV_.Destroy();
		resultTextureRTV_.Destroy();
		resultTextureSRV_.Destroy();
		resultTexture_.Destroy();

		copyPso_.Destroy();
		copyVS_.Destroy();
		copyPS_.Destroy();
		stateObject_.Destroy();

		rtDescMan_.Destroy();
		copyRootSig_.Destroy();
		rtLocalRootSig_.Destroy();
		rtGlobalRootSig_.Destroy();

		for (auto&& v : cmdLists_) v.Destroy();
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
	bool CreateRootSig(const D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** ppSig)
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* error = nullptr;
		bool ret = true;

		auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

		hr = device_.GetDeviceDep()->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(ppSig));
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

	D3D_ERROR:
		sl12::SafeRelease(blob);
		sl12::SafeRelease(error);
		return ret;
	}

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
			{ kRayGenName,				nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kClosestHitName,			nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kAnyHitName,				nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kMissName,				nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kMissShadowName,			nullptr, D3D12_EXPORT_FLAG_NONE },
		};
		dxrDesc.AddDxilLibrary(g_pTestLib, sizeof(g_pTestLib), libExport, ARRAYSIZE(libExport));

		// ヒットグループサブオブジェクト
		// Intersection, AnyHit, ClosestHitの組み合わせを定義し、ヒットグループ名でまとめるサブオブジェクトです.
		// マテリアルごとや用途ごと(マテリアル、シャドウなど)にサブオブジェクトを用意します.
		dxrDesc.AddHitGroup(kHitGroupName, true, kAnyHitName, kClosestHitName, nullptr);
		dxrDesc.AddHitGroup(kHitGroupShadowName, true, kAnyHitName, nullptr, nullptr);

		// シェーダコンフィグサブオブジェクト
		// ヒットシェーダ、ミスシェーダの引数となるPayload, IntersectionAttributesの最大サイズを設定します.
#if 0
		dxrDesc.AddShaderConfig(sizeof(float) * 4 + sizeof(sl12::u32) + sizeof(float), sizeof(float) * 2);
#else
		dxrDesc.AddShaderConfig(sizeof(float) * 4 + sizeof(float) * 3 * 3 + sizeof(sl12::u32), sizeof(float) * 2);
#endif

		// ローカルルートシグネチャサブオブジェクト
		// シェーダレコードごとに設定されるルートシグネチャを設定します.

		// Exports Assosiation サブオブジェクト
		// シェーダレコードとローカルルートシグネチャのバインドを行うサブオブジェクトです.
		LPCWSTR kExports[] = {
			kRayGenName,
			kMissName,
			kMissShadowName,
			kHitGroupName,
			kHitGroupShadowName,
		};
		dxrDesc.AddLocalRootSignatureAndExportAssociation(rtLocalRootSig_, kExports, ARRAYSIZE(kExports));

		// グローバルルートシグネチャサブオブジェクト
		// すべてのシェーダテーブルで参照されるグローバルなルートシグネチャを設定します.
		dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);

		// レイトレースコンフィグサブオブジェクト
		// TraceRay()を行うことができる最大深度を指定するサブオブジェクトです.
		dxrDesc.AddRaytracinConfig(2);

		// PSO生成
		if (!stateObject_.Initialize(&device_, dxrDesc))
		{
			return false;
		}

		return true;
	}

	bool CreateGeometry()
	{
#if !defined(USE_MEET_MAT)
		if (!glbMesh_.Initialize(&device_, &cmdLists_[0], "data/", "sponza.glb"))
#else
		if (!glbMesh_.Initialize(&device_, &cmdLists_[0], "data/", "MeetMat.glb"))
#endif
		{
			return false;
		}

		return true;
	}

	bool CreateAccelerationStructure()
	{
		// ASの生成はGPUで行うため、コマンドを積みGPUを動作させる必要があります.
		auto&& cmdList = cmdLists_[0];
		cmdList.Reset();

		// Bottom ASの生成準備
		std::vector< sl12::GeometryStructureDesc> geoDescs(glbMesh_.GetSubmeshCount());
		for (int i = 0; i < glbMesh_.GetSubmeshCount(); i++)
		{
			auto submesh = glbMesh_.GetSubmesh(i);
			geoDescs[i].InitializeAsTriangle(
				D3D12_RAYTRACING_GEOMETRY_FLAG_NONE,
				&submesh->GetPositionB(),
				&submesh->GetIndexB(),
				nullptr,
				submesh->GetPositionB().GetStride(),
				static_cast<UINT>(submesh->GetPositionB().GetSize() / submesh->GetPositionB().GetStride()),
				DXGI_FORMAT_R32G32B32_FLOAT,
				static_cast<UINT>(submesh->GetIndexB().GetSize()) / submesh->GetIndexB().GetStride(),
				DXGI_FORMAT_R32_UINT);
		}

		sl12::StructureInputDesc bottomInput{};
		if (!bottomInput.InitializeAsBottom(&device_, geoDescs.data(), glbMesh_.GetSubmeshCount(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		if (!bottomAS_.CreateBuffer(&device_, bottomInput.prebuildInfo.ResultDataMaxSizeInBytes, bottomInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}

		// コマンド発行
		if (!bottomAS_.Build(&cmdList, bottomInput))
		{
			return false;
		}

		// Top ASの生成準備
		sl12::StructureInputDesc topInput{};
		if (!topInput.InitializeAsTop(&device_, 1, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		sl12::TopInstanceDesc topInstance{};
		DirectX::XMFLOAT4X4 mtx;
#if !defined(USE_MEET_MAT)
		DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(20.0f, 20.0f, 20.0f);
#else
		DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(3.0f, 3.0f, 3.0f);
#endif
		DirectX::XMStoreFloat4x4(&mtx, scale);
		topInstance.Initialize(mtx, &bottomAS_);

		if (!topAS_.CreateBuffer(&device_, topInput.prebuildInfo.ResultDataMaxSizeInBytes, topInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}
		if (!topAS_.CreateInstanceBuffer(&device_, &topInstance, 1))
		{
			return false;
		}

		// コマンド発行
		if (!topAS_.Build(&cmdList, topInput, false))
		{
			return false;
		}

		// コマンド実行と終了待ち
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		bottomAS_.DestroyScratchBuffer();
		topAS_.DestroyScratchBuffer();
		topAS_.DestroyInstanceBuffer();

		return true;
	}

	bool CreateSceneCB()
	{
		// レイ生成シェーダで使用する定数バッファを生成する
		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			DirectX::XMLoadFloat4(&camPos_),
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 100.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

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
				DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
				cb->camPos = camPos_;
				cb->lightDir = lightDir;
				cb->lightColor = lightColor;
				cb->skyPower = skyPower_;
				cb->loopCount = 0;
				cb->maxBounces = maxBounces_;
				sceneCBs_[i].Unmap();

				if (!sceneCBVs_[i].Initialize(&device_, &sceneCBs_[i]))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool CreateShaderTable()
	{
		// LocalRS用のマテリアルDescriptorを用意する
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpu_handles;
		auto view_desc_size = rtDescMan_.GetViewDescSize();
		auto sampler_desc_size = rtDescMan_.GetSamplerDescSize();
		auto local_handle_start = rtDescMan_.IncrementLocalHandleStart();

		for (int i = 0; i < glbMesh_.GetSubmeshCount(); i++)
		{
			auto submesh = glbMesh_.GetSubmesh(i);
			auto material = glbMesh_.GetMaterial(submesh->GetMaterialIndex());
			auto texView = glbMesh_.GetTextureView(material->GetTexBaseColorIndex());
			auto texNView = glbMesh_.GetTextureView(material->GetTexNormalIndex());

			// CBVはなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);

			// SRVは6つ
			D3D12_CPU_DESCRIPTOR_HANDLE srv[] = {
				submesh->GetIndexBV().GetDescInfo().cpuHandle,
				submesh->GetPositionBV().GetDescInfo().cpuHandle,
				submesh->GetNormalBV().GetDescInfo().cpuHandle,
				submesh->GetTexcoordBV().GetDescInfo().cpuHandle,
				texView->GetDescInfo().cpuHandle,
				texNView->GetDescInfo().cpuHandle
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
		void* missShaderIdentifier[2];
		void* hitGroupShaderIdentifier[2];
		{
			ID3D12StateObjectProperties* prop;
			stateObject_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
			rayGenShaderIdentifier = prop->GetShaderIdentifier(kRayGenName);
			missShaderIdentifier[0] = prop->GetShaderIdentifier(kMissName);
			missShaderIdentifier[1] = prop->GetShaderIdentifier(kMissShadowName);
			hitGroupShaderIdentifier[0] = prop->GetShaderIdentifier(kHitGroupName);
			hitGroupShaderIdentifier[1] = prop->GetShaderIdentifier(kHitGroupShadowName);
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

		auto GenShaderTable = [&](void** shaderIds, int shaderIdsCount, sl12::Buffer& buffer, int count = 1)
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

					memcpy(p, shaderIds[id], shaderIdentifierSize);
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
		if (!GenShaderTable(missShaderIdentifier, 2, missTable_))
		{
			return false;
		}
		if (!GenShaderTable(hitGroupShaderIdentifier, 2, hitGroupTable_, glbMesh_.GetSubmeshCount()))
		{
			return false;
		}

		return true;
	}

	void UpdateSceneCB(int frameIndex)
	{
		auto mtxRot = DirectX::XMMatrixRotationY(camRotAngle_);
		auto cp = DirectX::XMLoadFloat4(&camPos_);
		cp = DirectX::XMVector4Transform(cp, mtxRot);

		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			cp,
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 100.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 0.0f, -1.0f, 0.0f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { lightColor_[0] * lightPower_, lightColor_[1] * lightPower_, lightColor_[2] * lightPower_, 1.0f };

		auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[frameIndex].Map(nullptr));
		DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
		DirectX::XMStoreFloat4(&cb->camPos, cp);
		cb->lightDir = lightDir;
		cb->lightColor = lightColor;
		cb->skyPower = skyPower_;
		cb->loopCount = loopCount_++;
		cb->maxBounces = maxBounces_;
		sceneCBs_[frameIndex].Unmap();
	}

private:
	static const int kBufferCount = sl12::Swapchain::kMaxBuffer;

	sl12::CommandList		cmdLists_[kBufferCount];
	sl12::RootSignature		rtGlobalRootSig_, rtLocalRootSig_;
	sl12::RaytracingDescriptorManager	rtDescMan_;
	sl12::DescriptorSet					rtGlobalDescSet_;

	sl12::DxrPipelineState		stateObject_;
	sl12::Texture				resultTexture_;
	sl12::TextureView			resultTextureSRV_;
	sl12::RenderTargetView		resultTextureRTV_;
	sl12::UnorderedAccessView	resultTextureUAV_;

	sl12::Buffer				randomBuffer_;
	sl12::BufferView			randomBufferSRV_;
	sl12::Buffer				seedBuffer_;
	sl12::UnorderedAccessView	seedBufferUAV_;

	sl12::GlbMesh			glbMesh_;
	sl12::Sampler			imageSampler_;

	sl12::BottomAccelerationStructure	bottomAS_;
	sl12::TopAccelerationStructure		topAS_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer			rayGenTable_, missTable_, hitGroupTable_;
	sl12::u64				shaderRecordSize_;

	std::vector<Sphere>		spheres_;
	sl12::Buffer			spheresAABB_;
	sl12::Buffer			instanceSB_;
	sl12::BufferView		instanceSBV_;

	sl12::Shader				copyVS_, copyPS_;
	sl12::RootSignature			copyRootSig_;
	sl12::GraphicsPipelineState	copyPso_;
	sl12::DescriptorSet			descSet_;

	sl12::Gui				gui_;
	sl12::InputData			inputData_{};

	sl12::Timestamp			gpuTimestamp_[sl12::Swapchain::kMaxBuffer];

#if !defined(USE_MEET_MAT)
	DirectX::XMFLOAT4		camPos_ = { -5.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, -5.0f, 0.0f, 1.0f };
#else
	DirectX::XMFLOAT4		camPos_ = { -5.0f, 3.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, 0.0f, 0.0f, 1.0f };
#endif
	DirectX::XMFLOAT4		upVec_ = { 0.0f, 1.0f, 0.0f, 0.0f };
	float					skyPower_ = 1.0f;
	float					lightColor_[3] = { 1.0f, 1.0f, 1.0f };
	float					lightPower_ = 1.0f;
	uint32_t				loopCount_ = 0;
	int						maxBounces_ = 4;
	float					camRotAngle_ = 0.0f;
	bool					isClearTarget_ = true;

	int		frameIndex_ = 0;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
