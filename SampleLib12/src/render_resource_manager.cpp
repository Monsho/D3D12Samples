#include <sl12/render_resource_manager.h>
#include <sl12/command_list.h>
#include <sl12/swapchain.h>


namespace sl12
{
	//-------------------------------------------
	// レンダリングリソースの初期化
	//-------------------------------------------
	bool RenderResource::Initialize(sl12::Device& device, const RenderResourceDesc& desc, sl12::u32 screenWidth, sl12::u32 screenHeight, D3D12_RESOURCE_STATES initialState)
	{
		// ミップレベルは明示する必要がある
		if (desc.mipLevels == 0)
		{
			return false;
		}
		// RTV、DSV、SRV、UAVはミップレベルに対して適切な数値でなければならない
		if ((desc.mipLevels < desc.targetCount)
			|| (desc.mipLevels + 1 < desc.srvCount)
			|| (desc.mipLevels < desc.uavCount))
		{
			return false;
		}

		desc_ = desc;

		// 幅と高さを決定する
		sl12::u32 width = desc.width;
		sl12::u32 height = desc.height;
		if (desc.resolution_rate > 0.0f)
		{
			width = static_cast<sl12::u32>(static_cast<float>(screenWidth) * desc.resolution_rate);
			height = static_cast<sl12::u32>(static_cast<float>(screenHeight) * desc.resolution_rate);
			desc_.width = desc_.height = 0;
		}

		// RTVかDSVかを決定する
		bool isRtv = desc.targetCount > 0;
		bool isDsv = false;
		if (isRtv)
		{
			if ((desc.format == DXGI_FORMAT_D16_UNORM)
				|| (desc.format == DXGI_FORMAT_D24_UNORM_S8_UINT)
				|| (desc.format == DXGI_FORMAT_D32_FLOAT)
				|| (desc.format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT))
			{
				isRtv = false;
				isDsv = true;
			}
		}

		// テクスチャオブジェクト生成
		{
			sl12::TextureDesc td{};
			td.dimension = sl12::TextureDimension::Texture2D;
			td.width = width;
			td.height = height;
			td.depth = 1;
			td.format = desc.format;
			td.mipLevels = desc.mipLevels;
			td.sampleCount = desc.sampleCount;
			td.initialState = initialState;
			td.clearDepth = 1.0f;
			td.isRenderTarget = isRtv;
			td.isDepthBuffer = isDsv;
			td.isUav = desc.uavCount > 0;
			if (!texture_.Initialize(&device, td))
			{
				return false;
			}
		}

		// RTV生成
		if (isRtv)
		{
			rtvs_.resize(desc.targetCount);
			for (sl12::u32 i = 0; i < desc.targetCount; ++i)
			{
				if (!rtvs_[i].Initialize(&device, &texture_, i))
				{
					return false;
				}
			}
		}
		// DSV生成
		else if (isDsv)
		{
			dsvs_.resize(desc.targetCount);
			for (sl12::u32 i = 0; i < desc.targetCount; ++i)
			{
				if (!dsvs_[i].Initialize(&device, &texture_, i))
				{
					return false;
				}
			}
		}

		// SRV生成
		if (desc.srvCount > 0)
		{
			srvs_.resize(desc.srvCount);

			// 0番はフルミップレベル
			if (!srvs_[0].Initialize(&device, &texture_))
			{
				return false;
			}

			// 1番以降はミップレベル限定
			for (sl12::u32 i = 1; i < desc.srvCount; ++i)
			{
				if (!srvs_[i].Initialize(&device, &texture_, i - 1, 1))
				{
					return false;
				}
			}
		}

		// UAV生成
		if (desc.uavCount)
		{
			uavs_.resize(desc.uavCount);
			for (sl12::u32 i = 0; i < desc.uavCount; ++i)
			{
				if (!uavs_[i].Initialize(&device, &texture_, i))
				{
					return false;
				}
			}
		}

		state_ = initialState;

		return true;
	}

	//-------------------------------------------
	// レンダリングリソースの解放
	//-------------------------------------------
	void RenderResource::Destroy()
	{
		for (auto&& v : rtvs_) v.Destroy();
		for (auto&& v : dsvs_) v.Destroy();
		for (auto&& v : srvs_) v.Destroy();
		for (auto&& v : uavs_) v.Destroy();
		texture_.Destroy();
	}


	//-------------------------------------------
	// レンダリングリソースマネージャの初期化
	//-------------------------------------------
	bool RenderResourceManager::Initialize(sl12::Device& device, sl12::u32 screenWidth, sl12::u32 screenHeight)
	{
		if (screenWidth == 0 || screenHeight == 0)
		{
			return false;
		}

		pDevice_ = &device;
		screenWidth_ = screenWidth;
		screenHeight_ = screenHeight;

		return true;
	}

	//-------------------------------------------
	// レンダリングリソースマネージャの解放
	//-------------------------------------------
	void RenderResourceManager::Destroy()
	{
		AllReset();
		pDevice_ = nullptr;
	}

	//-------------------------------------------
	// 全リソースリセット
	//-------------------------------------------
	void RenderResourceManager::AllReset()
	{
		for (auto&& v : resources_)
		{
			delete v;
		}
		resources_.clear();
		resource_map_.clear();
	}

	namespace
	{
		typedef std::map<ResourceID, sl12::u32>		LastAccessPassInfo;

		void MakeResourceHistory(std::vector<ResourceProducerBase*>& producers, LastAccessPassInfo& lastAccess)
		{
			auto prodCnt = producers.size();
			sl12::u16 passNo = 0;
			ResourceProducerBase* prev_prod = nullptr;
			for (auto&& prod : producers)
			{
				// 出力リソースを処理する
				auto cnt = prod->GetOutputCount();
				auto ids = prod->GetOutputIds();
				for (sl12::u16 i = 0; i < cnt; ++i)
				{
					// IDの加工を行う
					// 特定用途を持たないただの出力バッファ(kPrevOutputID)の場合、パス番号と出力番号からユニークなIDを生成する
					auto id = ids[i];
					if (id.isPrevOutput)
					{
						id = ResourceID::CreatePrevOutputID((sl12::u8)passNo, (sl12::u8)i);
						prod->SetOutputID(i, id);		// IDをセットし直す
					}

					lastAccess[id] = passNo;
				}

				// 入力リソースを処理する
				cnt = prod->GetInputCount();
				ids = prod->GetInputIds();
				for (sl12::u16 i = 0; i < cnt; ++i)
				{
					// IDの加工を行う
					// kPrevOutputID以上の場合は前回パスの出力を用いる
					// 入力IDとしては (kPrevOutputID | prevOutputIndex) を指定するものとする
					auto id = ids[i];
					if (id.isPrevOutput)
					{
						assert(prev_prod != nullptr);
						assert(id.index < prev_prod->GetOutputCount());

						id = ResourceID::CreatePrevOutputID(passNo - 1, id.index);
						prod->SetInput(i, id);			// IDをセットし直す
					}

					lastAccess[id] = passNo;
				}

				// 一時リソースに自動的にIDを割り当てる
				cnt = prod->GetTempCount();
				for (sl12::u16 i = 0; i < cnt; ++i)
				{
					auto id = ResourceID::CreateTemporalID((sl12::u8)passNo, (sl12::u8)i);
					prod->SetTempID(i, id);
					lastAccess[id] = passNo;
				}

				prev_prod = prod;
				passNo++;
			}
		}

		// 未使用リソースから使えるものを探す
		std::vector<RenderResource*>::iterator FindFromUnusedResource(std::vector<RenderResource*>& unusedRes, const RenderResourceDesc& desc)
		{
			for (auto it = unusedRes.begin(); it != unusedRes.end(); it++)
			{
				if ((*it)->IsSameDesc(desc))
				{
					return it;
				}
			}
			return unusedRes.end();
		}

		// ヒストリーバッファの記述子を検索する
		bool FindDescForHistoryBuffer(RenderResourceDesc* pOut, const std::vector<ResourceProducerBase*>& producers, ResourceID id)
		{
			id.historyOffset = 0;
			for (auto&& prod : producers)
			{
				// 出力バッファを検索する
				auto cnt = prod->GetOutputCount();
				auto ids = prod->GetOutputIds();
				for (u32 i = 0; i < cnt; i++)
				{
					if (ids[i] == id)
					{
						*pOut = prod->GetOutputDescs()[i];
						return true;
					}
				}
			}
			return false;
		}

		// リソース生成
		void MakeResourcesDetail(
			sl12::Device& device,
			sl12::u32 screenWidth, sl12::u32 screenHeight,
			const LastAccessPassInfo& lastAccess,
			std::vector<ResourceProducerBase*>& producers,
			std::vector<RenderResource*>& outResources,
			std::map<ResourceID, RenderResource*>& outResourceMap)
		{
			const D3D12_RESOURCE_STATES kInitialState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			const D3D12_RESOURCE_STATES kInputState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			const D3D12_RESOURCE_STATES kRtvState = D3D12_RESOURCE_STATE_RENDER_TARGET;
			const D3D12_RESOURCE_STATES kDsvState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

			sl12::u16 passNo = 0;
			std::map<ResourceID, RenderResource*> usedRes;
			std::vector<RenderResource*> unusedRes;

			// 前回フレームのリソースを使用中リストと未使用リストに振り分ける
			outResourceMap.clear();
			for (auto&& res : outResources)
			{
				if (res->IsHistoryEnd())
				{
					unusedRes.push_back(res);
				}
				else
				{
					auto id = res->GetLastID();
					id.historyOffset++;
					res->IncrementHistory();
					usedRes[id] = res;
					outResourceMap[id] = res;
				}
			}

			for (auto&& prod : producers)
			{
				// 入力リソースを検索・生成する
				auto cnt = prod->GetInputCount();
				auto ids = prod->GetInputIds();
				for (sl12::u32 i = 0; i < cnt; i++)
				{
					auto id = ids[i];
					auto findIt = usedRes.find(id);
					if (findIt != usedRes.end())
					{
						// 使用中リストに見つかったので状態を保存
						auto res = findIt->second;
						prod->SetInputPrevState(i, res->GetState());
						res->SetState(kInputState);
					}
					else
					{
						// まだ描画されていないヒストリーバッファの場合は使用中リストに見つからない
						assert(id.historyOffset > 0);
						// 入力リソースには記述子が指定されていないので、プロデューサーから対応する記述子を検索する
						// NOTE: ヒストリーバッファに描画するパスが存在しないはずはないが、存在しない場合はAssertする
						RenderResourceDesc desc;
						bool isFindHBDesc = FindDescForHistoryBuffer(&desc, producers, id);
						assert(isFindHBDesc);
						// 新規リソースを作成する
						auto res = new RenderResource();
						res->Initialize(device, desc, screenWidth, screenHeight, kInitialState);
						res->SetHistoryMax(0);
						res->SetLastID(id);
						outResources.push_back(res);
						usedRes[id] = res;
						outResourceMap[id] = res;
						prod->SetOutputPrevState(i, kInitialState);
					}
				}

				// 出力リソースを検索・生成する
				cnt = prod->GetOutputCount();
				ids = prod->GetOutputIds();
				auto descs = prod->GetOutputDescs();
				for (sl12::u32 i = 0; i < cnt; i++)
				{
					auto id = ids[i];
					auto findIt = usedRes.find(id);
					if (findIt != usedRes.end())
					{
						// リソースが使用中リストから見つかった
						auto res = findIt->second;
						if (!id.isSwapchain)
						{
							prod->SetOutputPrevState(i, res->GetState());
							res->SetState(res->IsRtv() ? kRtvState : kDsvState);
						}
						else
						{
							prod->SetOutputPrevState(i, kRtvState);
						}
					}
					else if (id.isSwapchain)
					{
						// スワップチェインを使用するパスが初めて登場したので、前回状態はPresentとする
						// NOTE: スワップチェインはヒストリーを無視する
						prod->SetOutputPrevState(i, D3D12_RESOURCE_STATE_PRESENT);
						usedRes[id] = nullptr;
					}
					else
					{
						RenderResource* res;
						auto&& desc = descs[i];

						// 未使用リソースに使えるものがあるかチェック
						auto unusedIt = FindFromUnusedResource(unusedRes, desc);
						if (unusedIt != unusedRes.end())
						{
							// 未使用リソースが見つかったので利用する
							res = *unusedIt;
							unusedRes.erase(unusedIt);
							prod->SetOutputPrevState(i, res->GetState());
						}
						else
						{
							// 未使用リソースが存在しないので新規作成
							res = new RenderResource();
							res->Initialize(device, descs[i], screenWidth, screenHeight, kInitialState);
							outResources.push_back(res);
							prod->SetOutputPrevState(i, kInitialState);
						}

						// 使用中リストに登録する
						res->SetState(res->IsRtv() ? kRtvState : kDsvState);
						res->SetHistoryMax(desc.historyMax);
						res->SetLastID(id);
						usedRes[id] = res;
						outResourceMap[id] = res;
					}
				}

				// 一時リソースを検索・生成する
				cnt = prod->GetTempCount();
				ids = prod->GetTempIds();
				descs = prod->GetTempDescs();
				for (sl12::u32 i = 0; i < cnt; i++)
				{
					RenderResource* res;

					auto id = ids[i];
					auto unusedIt = FindFromUnusedResource(unusedRes, descs[i]);
					if (unusedIt != unusedRes.end())
					{
						// 未使用リソースが見つかったので利用する
						res = *unusedIt;
						unusedRes.erase(unusedIt);
						prod->SetTempPrevState(i, res->GetState());
					}
					else
					{
						// 未使用リソースが存在しないので新規作成
						res = new RenderResource();
						res->Initialize(device, descs[i], screenWidth, screenHeight, kInitialState);
						outResources.push_back(res);
						prod->SetTempPrevState(i, kInitialState);
					}

					// ワークを使用マップに登録する
					res->SetState(kInputState);
					res->SetHistoryMax(0);
					res->SetLastID(id);
					usedRes[id] = res;
					outResourceMap[id] = res;
				}

				// 使用中リストを整理する
				auto usedIt = usedRes.begin();
				while (usedIt != usedRes.end())
				{
					if (usedIt->first.isSwapchain)
					{
						usedIt++;
						continue;
					}

					auto findIt = lastAccess.find(usedIt->first);
					if (usedIt->second->IsHistoryEnd() && (findIt != lastAccess.end()) && (findIt->second <= passNo))
					{
						// すでに不要になったリソースを未使用リストに移動する
						// NOTE: ヒストリーバッファは保持するフレーム数を経過するまで未使用にできない
						unusedRes.push_back(usedIt->second);
						auto thisIt = usedIt;
						usedIt++;
						usedRes.erase(thisIt);
					}
					else
					{
						usedIt++;
					}
				}

				passNo++;
			}
		}

	}

	//-------------------------------------------
	// 依存関係から必要なリソースを生成する
	//-------------------------------------------
	void RenderResourceManager::MakeResources(std::vector<ResourceProducerBase*>& producers)
	{
		assert(pDevice_ != nullptr);

		// 各IDの最終アクセスパス番号を記録する
		LastAccessPassInfo lastAccess;
		MakeResourceHistory(producers, lastAccess);

		// 実際のリソース生成
		MakeResourcesDetail(*pDevice_, screenWidth_, screenHeight_, lastAccess, producers, resources_, resource_map_);
	}

	//-------------------------------------------
	// 入力リソースにバリアを張る
	//-------------------------------------------
	void RenderResourceManager::BarrierInputResources(CommandList& cmdList, const ResourceProducerBase* pProd)
	{
		auto count = pProd->GetInputCount();
		auto ids = pProd->GetInputIds();
		auto end = ids + count;
		auto prevState = pProd->GetInputPrevStates();
		for (; ids != end; ids++)
		{
			if (*prevState != (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
			{
				auto res = GetRenderResourceFromID(*ids);
				cmdList.TransitionBarrier(res->GetTexture(), *prevState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			prevState++;
		}
	}

	//-------------------------------------------
	// 出力リソースにバリアを張る
	//-------------------------------------------
	void RenderResourceManager::BarrierOutputResources(CommandList& cmdList, const ResourceProducerBase* pProd)
	{
		auto count = pProd->GetOutputCount();
		auto ids = pProd->GetOutputIds();
		auto end = ids + count;
		auto prevState = pProd->GetOutputPrevStates();
		for (; ids != end; ids++)
		{
			if (ids->isSwapchain)
			{
				if (*prevState == D3D12_RESOURCE_STATE_PRESENT)
				{
					auto scTex = pDevice_->GetSwapchain().GetCurrentTexture(1);
					cmdList.TransitionBarrier(scTex, *prevState, D3D12_RESOURCE_STATE_RENDER_TARGET);
				}
			}
			else
			{
				auto res = GetRenderResourceFromID(*ids);
				if (res->IsRtv() && (*prevState != D3D12_RESOURCE_STATE_RENDER_TARGET))
				{
					cmdList.TransitionBarrier(res->GetTexture(), *prevState, D3D12_RESOURCE_STATE_RENDER_TARGET);
				}
				else if (res->IsDsv() && (*prevState != D3D12_RESOURCE_STATE_DEPTH_WRITE))
				{
					cmdList.TransitionBarrier(res->GetTexture(), *prevState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				}
			}
			prevState++;
		}
	}

}	// namespace sl12


//	EOF
