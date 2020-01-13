#include <vector>

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
#include "sl12/glb_mesh.h"
#include "sl12/descriptor_set.h"
#include "sl12/fence.h"

#include "CompiledShaders/test.lib.hlsl.h"


namespace
{
	static const int	kScreenWidth = 1280;
	static const int	kScreenHeight = 720;

	static LPCWSTR		kRayGenName			= L"RayGenerator";
	static LPCWSTR		kClosestHitName		= L"ClosestHitProcessor";
	static LPCWSTR		kMissName			= L"MissProcessor";
	static LPCWSTR		kHitGroupName		= L"HitGroup";

	static const sl12::u32	kMipLevelColors[] = {
		0xffffffff,
		0xff0000ff,
		0xff00ff00,
		0xffff0000,
		0xffffff00,
		0xff00ffff,
		0xffff00ff,
		0xff3f007f,
		0xff007f3f,
		0xff7f3f00,
		0xff7f003f,
		0xff003f7f,
		0xff3f7f00,
	};
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

		cmdLists_[0].Reset();

		// テクスチャ読み込み
		{
			sl12::File texFile("data/ConcreteTile_basecolor.tga");

			if (!imageTexture_.InitializeFromTGA(&device_, &cmdLists_[0], texFile.GetData(), texFile.GetSize(), 1, false))
			{
				return false;
			}
			if (!imageTextureView_.Initialize(&device_, &imageTexture_))
			{
				return false;
			}

			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			if (!imageSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}

		// Tiled Resourceの初期化
		{
			D3D12_RESOURCE_DESC desc = {};
			desc.Width = 1024;
			desc.Height = 1024;
			desc.MipLevels = 11;
			desc.DepthOrArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			desc.SampleDesc.Count = 1;
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;

			HRESULT hr = device_.GetDeviceDep()->CreateReservedResource(
				&desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&pTiledTexture_));
			assert(SUCCEEDED(hr));

			auto numSubresources = desc.MipLevels;
			sl12::u64 totalSize;
			tiledFootprints_.resize(numSubresources);
			tiledNumRows_.resize(numSubresources);
			tiledRowSizes_.resize(numSubresources);
			device_.GetDeviceDep()->GetCopyableFootprints(&desc, 0, numSubresources, 0, tiledFootprints_.data(), tiledNumRows_.data(), tiledRowSizes_.data(), &totalSize);

			UINT numTiles = 0;
			D3D12_TILE_SHAPE tile_shape = {};
			UINT subresource_count = desc.MipLevels;
			std::vector<D3D12_SUBRESOURCE_TILING> tilings(subresource_count);
			device_.GetDeviceDep()->GetResourceTiling(pTiledTexture_, &numTiles, &packedMipInfo_, &tile_shape, &subresource_count, 0, &tilings[0]);
			assert(numTiles > 0);

			D3D12_HEAP_DESC hd{};
			hd.SizeInBytes = numTiles * D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			hd.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
			hd.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			hd.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			hd.Properties.CreationNodeMask = 0x01;
			hd.Properties.VisibleNodeMask = 0x01;
			hd.Alignment = 0;
			hd.Flags = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
			hr = device_.GetDeviceDep()->CreateHeap(&hd, IID_PPV_ARGS(&pTiledHeap_));
			assert(SUCCEEDED(hr));

			D3D12_SHADER_RESOURCE_VIEW_DESC vd{};
			vd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			vd.Format = desc.Format;
			vd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			vd.Texture2D.MostDetailedMip = 0;
			vd.Texture2D.MipLevels = desc.MipLevels;
			vd.Texture2D.PlaneSlice = 0;
			vd.Texture2D.ResourceMinLODClamp = 0.0f;

			tiledDescInfo_ = device_.GetViewDescriptorHeap().Allocate();
			if (!tiledDescInfo_.IsValid())
			{
				return false;
			}

			device_.GetDeviceDep()->CreateShaderResourceView(pTiledTexture_, &vd, tiledDescInfo_.cpuHandle);
		}

		// Tiled Resourceの更新
		{
			// 最低MipTileの割当を行う
			// Packed Tileは(基本的に)1つのタイルに対して複数のサブリソースが割り当てられている状態
			// 高いミップレベル(解像度が低い)に対してサブリソース分のタイル分割は無駄が多いため、一定以下のミップレベルについては1つのタイルで賄ってしまうようになっている
			// どのレベルからPacked Tileになるかはハードウェアドライバ次第？
			D3D12_TILED_RESOURCE_COORDINATE start_coordinates[1]{};
			start_coordinates[0].Subresource = packedMipInfo_.NumStandardMips;

			D3D12_TILE_REGION_SIZE region_sizes[1]{};
			region_sizes[0].NumTiles = packedMipInfo_.NumTilesForPackedMips;

			D3D12_TILE_RANGE_FLAGS range_flags[1]{};
			range_flags[0] = D3D12_TILE_RANGE_FLAG_NONE;

			UINT start_offsets[1] = { 0 };
			UINT range_tile_counts[1] = { 1 };

			device_.GetGraphicsQueue().GetQueueDep()->UpdateTileMappings(
				pTiledTexture_,
				1,
				&start_coordinates[0],
				&region_sizes[0],
				pTiledHeap_,
				1,
				&range_flags[0],
				&start_offsets[0],
				&range_tile_counts[0],
				D3D12_TILE_MAPPING_FLAG_NONE);

			// Packed Tileに対してコピー命令発行
			{
				// アップロード用のオブジェクトを作成
				D3D12_HEAP_PROPERTIES heapProp = {};
				heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
				heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heapProp.CreationNodeMask = 1;
				heapProp.VisibleNodeMask = 1;

				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Alignment = 0;
				desc.Width = 1024 * 1024 * 4;
				desc.Height = 1;
				desc.DepthOrArraySize = 1;
				desc.MipLevels = 1;
				desc.Format = DXGI_FORMAT_UNKNOWN;
				desc.SampleDesc.Count = 1;
				desc.SampleDesc.Quality = 0;
				desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				desc.Flags = D3D12_RESOURCE_FLAG_NONE;

				ID3D12Resource* pSrcImage{ nullptr };
				auto hr = device_.GetDeviceDep()->CreateCommittedResource(
					&heapProp,
					D3D12_HEAP_FLAG_NONE,
					&desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&pSrcImage));
				if (FAILED(hr))
				{
					return false;
				}

				// リソースをマップしてイメージ情報をコピー
				sl12::u8* pData = nullptr;
				hr = pSrcImage->Map(0, NULL, reinterpret_cast<void**>(&pData));
				for (size_t m = packedMipInfo_.NumStandardMips; m < tiledFootprints_.size(); m++)
				{
					auto color = kMipLevelColors[m];
					auto size = tiledFootprints_[m].Footprint.RowPitch * tiledNumRows_[m];
					for (; size > 0; size -= sizeof(color))
					{
						memcpy(pData, &color, sizeof(color));
						pData += sizeof(color);
					}
				}
				pSrcImage->Unmap(0, nullptr);

				// コピー命令を発行
				sl12::u64 offset = 0;
				for (size_t m = packedMipInfo_.NumStandardMips; m < tiledFootprints_.size(); m++)
				{
					D3D12_TEXTURE_COPY_LOCATION src, dst;
					src.pResource = pSrcImage;
					src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					src.PlacedFootprint.Footprint = tiledFootprints_[m].Footprint;
					src.PlacedFootprint.Offset = offset;
					dst.pResource = pTiledTexture_;
					dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					dst.SubresourceIndex = m;
					cmdLists_[0].GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

					offset += tiledFootprints_[m].Footprint.RowPitch * tiledNumRows_[m];
				}

				// コピーに使用したStagingリソースは破棄対象に
				device_.PendingKill(new sl12::ReleaseObjectItem<ID3D12Resource>(pSrcImage));
			}
		}

		// ルートシグネチャの初期化
		if (!sl12::CreateRaytracingRootSignature(&device_, 1, 1, 0, 1, 0, &rtGlobalRootSig_, &rtLocalRootSig_))
		{
			return false;
		}

		// パイプラインステートオブジェクトの初期化
		if (!CreatePipelineState())
		{
			return false;
		}

		// 出力先のテクスチャを生成
		{
			sl12::TextureDesc desc;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.mipLevels = 1;
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			desc.sampleCount = 1;
			desc.clearColor[4] = { 0.0f };
			desc.clearDepth = 1.0f;
			desc.clearStencil = 0;
			desc.isRenderTarget = false;
			desc.isDepthBuffer = false;
			desc.isUav = true;
			if (!resultTexture_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!resultTextureView_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}
		}

		// ジオメトリを生成する
		if (!CreateGeometry())
		{
			return false;
		}

		cmdLists_[0].Close();
		cmdLists_[0].Execute();
		device_.WaitDrawDone();

		// Raytracing用DescriptorHeapの初期化
		if (!rtDescMan_.Initialize(&device_, 1, 1, 1, 0, 1, 0, glbMesh_.GetSubmeshCount()))
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

		return true;
	}

	bool Execute() override
	{
		device_.WaitPresent();
		device_.SyncKillObjects();

		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto&& cmdList = cmdLists_[frameIndex];
		auto&& d3dCmdList = cmdList.GetCommandList();
		auto&& dxrCmdList = cmdList.GetDxrCommandList();

		UpdateSceneCB(frameIndex);

		cmdList.Reset();

		auto&& swapchain = device_.GetSwapchain();
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
		d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView()->GetDescInfo().cpuHandle, color, 0, nullptr);

		// デスクリプタを設定
		rtGlobalDescSet_.Reset();
		rtGlobalDescSet_.SetCsCbv(0, sceneCBVs_[frameIndex].GetDescInfo().cpuHandle);
		rtGlobalDescSet_.SetCsUav(0, resultTextureView_.GetDescInfo().cpuHandle);

		// コピーしつつコマンドリストに積む
		D3D12_GPU_VIRTUAL_ADDRESS as_address[] = {
			topAS_.GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress(),
		};
		cmdList.SetRaytracingGlobalRootSignatureAndDescriptorSet(&rtGlobalRootSig_, &rtGlobalDescSet_, &rtDescMan_, as_address, ARRAYSIZE(as_address));

		// レイトレースを実行
		D3D12_DISPATCH_RAYS_DESC desc{};
		desc.HitGroupTable.StartAddress = hitGroupTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.HitGroupTable.SizeInBytes = hitGroupTable_.GetSize();
		desc.HitGroupTable.StrideInBytes = desc.HitGroupTable.SizeInBytes;
		desc.MissShaderTable.StartAddress = missTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.MissShaderTable.SizeInBytes = missTable_.GetSize();
		desc.MissShaderTable.StrideInBytes = desc.MissShaderTable.SizeInBytes;
		desc.RayGenerationShaderRecord.StartAddress = rayGenTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.RayGenerationShaderRecord.SizeInBytes = rayGenTable_.GetSize();
		desc.Width = kScreenWidth;
		desc.Height = kScreenHeight;
		desc.Depth = 1;
		dxrCmdList->SetPipelineState1(stateObject_.GetPSO());
		dxrCmdList->DispatchRays(&desc);

		cmdList.UAVBarrier(&resultTexture_);

		// リソースバリア
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

		d3dCmdList->CopyResource(swapchain.GetCurrentTexture()->GetResourceDep(), resultTexture_.GetResourceDep());

		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

		topAS_.Destroy();
		bottomAS_.Destroy();

		glbMesh_.Destroy();
		geometryIBV_.Destroy();
		geometryIB_.Destroy();
		geometryUVBV_.Destroy();
		geometryUVB_.Destroy();
		geometryVB_.Destroy();

		resultTextureView_.Destroy();
		resultTexture_.Destroy();

		stateObject_.Destroy();

		rtDescMan_.Destroy();
		rtLocalRootSig_.Destroy();
		rtGlobalRootSig_.Destroy();

		imageSampler_.Destroy();
		imageTextureView_.Destroy();
		imageTexture_.Destroy();

		for (auto&& v : cmdLists_) v.Destroy();
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
		dxrDesc.AddDxilLibrary(g_pTestLib, sizeof(g_pTestLib), libExport, ARRAYSIZE(libExport));

		// ヒットグループサブオブジェクト
		// Intersection, AnyHit, ClosestHitの組み合わせを定義し、ヒットグループ名でまとめるサブオブジェクトです.
		// マテリアルごとや用途ごと(マテリアル、シャドウなど)にサブオブジェクトを用意します.
		dxrDesc.AddHitGroup(kHitGroupName, true, nullptr, kClosestHitName, nullptr);

		// シェーダコンフィグサブオブジェクト
		// ヒットシェーダ、ミスシェーダの引数となるPayload, IntersectionAttributesの最大サイズを設定します.
		dxrDesc.AddShaderConfig(sizeof(float) * 4, sizeof(float) * 2);

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
		//dxrDesc.AddLocalRootSignatureAndExportAssociation(localRootSig_, kExports, ARRAYSIZE(kExports));

		// グローバルルートシグネチャサブオブジェクト
		// すべてのシェーダテーブルで参照されるグローバルなルートシグネチャを設定します.
		dxrDesc.AddGlobalRootSignature(rtGlobalRootSig_);
		//dxrDesc.AddGlobalRootSignature(globalRootSig_);

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
		const float size = 2.f;
		float vertices[] = {
			-size,  size, -size,
			 size,  size, -size,
			-size,  size,  size,
			 size,  size,  size,

			 size, -size, -size,
			-size, -size, -size,
			 size, -size,  size,
			-size, -size,  size,

			 size,  size, -size,
			 size,  size,  size,
			 size, -size, -size,
			 size, -size,  size,

			-size,  size, -size,
			-size,  size,  size,
			-size, -size, -size,
			-size, -size,  size,

			-size,  size, -size,
			 size,  size, -size,
			-size, -size, -size,
			 size, -size, -size,

			-size,  size,  size,
			 size,  size,  size,
			-size, -size,  size,
			 size, -size,  size,
		};
		float uv[] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 1.0f,

			1.0f, 0.0f,
			0.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,

			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			0.0f, 1.0f,

			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			0.0f, 1.0f,

			0.0f, 1.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,

			0.0f, 1.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
		};
		UINT32 indices[] =
		{
			0, 2, 1, 1, 2, 3,
			4, 6, 5, 5, 6, 7,
			8, 9, 10, 9, 11, 10,
			12, 14, 13, 13, 14, 15,
			16, 17, 18, 17, 19, 18,
			20, 22, 21, 21, 22, 23,
		};

		if (!geometryVB_.Initialize(&device_, sizeof(vertices), sizeof(float) * 3, sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}
		if (!geometryUVB_.Initialize(&device_, sizeof(uv), sizeof(float) * 2, sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}
		if (!geometryIB_.Initialize(&device_, sizeof(indices), sizeof(indices[0]), sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}

		void* p = geometryVB_.Map(nullptr);
		memcpy(p, vertices, sizeof(vertices));
		geometryVB_.Unmap();

		p = geometryUVB_.Map(nullptr);
		memcpy(p, uv, sizeof(uv));
		geometryUVB_.Unmap();

		p = geometryIB_.Map(nullptr);
		memcpy(p, indices, sizeof(indices));
		geometryIB_.Unmap();

		if (!geometryIBV_.Initialize(&device_, &geometryIB_, 0, 0, 0))
		{
			return false;
		}
		if (!geometryUVBV_.Initialize(&device_, &geometryUVB_, 0, 0, sizeof(float) * 2))
		{
			return false;
		}

		if (!glbMesh_.Initialize(&device_, &cmdLists_[0], "data/", "PreviewSphere.glb"))
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
		sl12::GeometryStructureDesc geoDesc{};
		auto submesh = glbMesh_.GetSubmesh(0);
		geoDesc.InitializeAsTriangle(
			D3D12_RAYTRACING_GEOMETRY_FLAG_NONE,
			&submesh->GetPositionB(),
			&submesh->GetIndexB(),
			nullptr,
			submesh->GetPositionB().GetStride(),
			static_cast<UINT>(submesh->GetPositionB().GetSize() / submesh->GetPositionB().GetStride()),
			DXGI_FORMAT_R32G32B32_FLOAT,
			static_cast<UINT>(submesh->GetIndexB().GetSize()) / submesh->GetIndexB().GetStride(),
			DXGI_FORMAT_R32_UINT);
		//geoDesc.InitializeAsTriangle(
		//	&geometryVB_,
		//	&geometryIB_,
		//	nullptr,
		//	geometryVB_.GetStride(),
		//	static_cast<UINT>(geometryVB_.GetSize() / geometryVB_.GetStride()),
		//	DXGI_FORMAT_R32G32B32_FLOAT,
		//	static_cast<UINT>(geometryIB_.GetSize()) / geometryIB_.GetStride(),
		//	DXGI_FORMAT_R32_UINT);

		sl12::StructureInputDesc bottomInput{};
		if (!bottomInput.InitializeAsBottom(&device_, &geoDesc, 1, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
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
		topInstance.Initialize(&bottomAS_);

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

		DirectX::XMFLOAT4 lightDir = { 1.0f, -1.0f, -1.0f, 0.0f };
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
			// CBVはなし
			gpu_handles.push_back(local_handle_start.viewGpuHandle);

			// SRVは3つ
			D3D12_CPU_DESCRIPTOR_HANDLE srv[] = {
				//imageTextureView_.GetDescInfo().cpuHandle,
				tiledDescInfo_.cpuHandle,
				glbMesh_.GetSubmesh(i)->GetTexcoordBV().GetDescInfo().cpuHandle,
				glbMesh_.GetSubmesh(i)->GetIndexBV().GetDescInfo().cpuHandle,
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
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier;
		{
			ID3D12StateObjectProperties* prop;
			stateObject_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
			rayGenShaderIdentifier = prop->GetShaderIdentifier(kRayGenName);
			missShaderIdentifier = prop->GetShaderIdentifier(kMissName);
			hitGroupShaderIdentifier = prop->GetShaderIdentifier(kHitGroupName);
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

		auto GenShaderTable = [&](void* shaderId, sl12::Buffer& buffer)
		{
			if (!buffer.Initialize(&device_, shaderRecordSize, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer.Map(nullptr));
			memcpy(p, shaderId, shaderIdentifierSize);
			p += descHandleOffset;

			memcpy(p, gpu_handles.data(), sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * 4);
			buffer.Unmap();

			return true;
		};

		if (!GenShaderTable(rayGenShaderIdentifier, rayGenTable_))
		{
			return false;
		}
		if (!GenShaderTable(missShaderIdentifier, missTable_))
		{
			return false;
		}
		if (!GenShaderTable(hitGroupShaderIdentifier, hitGroupTable_))
		{
			return false;
		}

		return true;
	}

	void UpdateSceneCB(int frameIndex)
	{
		auto mtxRot = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(1.0f));
		auto cp = DirectX::XMLoadFloat4(&camPos_);
		cp = DirectX::XMVector4Transform(cp, mtxRot);
		DirectX::XMStoreFloat4(&camPos_, cp);

		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			cp,
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 100.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 1.0f, -1.0f, -1.0f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[frameIndex].Map(nullptr));
		DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
		cb->camPos = camPos_;
		cb->lightDir = lightDir;
		cb->lightColor = lightColor;
		sceneCBs_[frameIndex].Unmap();
	}

private:
	static const int kBufferCount = sl12::Swapchain::kMaxBuffer;

	sl12::CommandList		cmdLists_[kBufferCount];
	sl12::RootSignature		rtGlobalRootSig_, rtLocalRootSig_;
	sl12::RaytracingDescriptorManager	rtDescMan_;
	sl12::DescriptorSet		rtGlobalDescSet_;

	sl12::DxrPipelineState		stateObject_;
	sl12::Texture				resultTexture_;
	sl12::UnorderedAccessView	resultTextureView_;

	sl12::Texture			imageTexture_;
	sl12::TextureView		imageTextureView_;
	sl12::Sampler			imageSampler_;

	sl12::GlbMesh			glbMesh_;
	sl12::Buffer			geometryVB_, geometryIB_, geometryUVB_;
	sl12::BufferView		geometryIBV_, geometryUVBV_;

	sl12::BottomAccelerationStructure	bottomAS_;
	sl12::TopAccelerationStructure		topAS_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer			rayGenTable_, missTable_, hitGroupTable_;

	DirectX::XMFLOAT4		camPos_ = { 5.0f, 5.0f, -5.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		upVec_ = { 0.0f, 1.0f, 0.0f, 0.0f };

	int		frameIndex_ = 0;

	ID3D12Resource*			pTiledTexture_ = nullptr;
	ID3D12Heap*				pTiledHeap_ = nullptr;
	sl12::DescriptorInfo	tiledDescInfo_;
	D3D12_PACKED_MIP_INFO	packedMipInfo_;
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>	tiledFootprints_;
	std::vector<sl12::u32>	tiledNumRows_;
	std::vector<sl12::u64>	tiledRowSizes_;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
