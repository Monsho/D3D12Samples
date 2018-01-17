#include <sl12/render_resource_manager.h>
#include <sl12/command_list.h>
#include <sl12/swapchain.h>


namespace sl12
{
	//-------------------------------------------
	// レンダリングリソースの初期化
	//-------------------------------------------
	bool RenderResource::Initialize(sl12::Device& device, const RenderResourceDesc& desc, sl12::u32 screenWidth, sl12::u32 screenHeight)
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
			td.initialState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
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
		struct RWIndex
		{
			bool		isRead = false;
			sl12::u32	producerNo = 0;

			RWIndex(bool is_read, sl12::u32 prod_no)
				: isRead(is_read), producerNo(prod_no)
			{}
		};	// struct RWIndex

		struct RWHistory
		{
			std::vector<RWIndex>	history;

			RWHistory()
			{}
			RWHistory(size_t s)
			{
				history.reserve(s);
			}
		};	// struct RWHistory

		struct ResourceWork
		{
			RenderResource*			p_res;
			D3D12_RESOURCE_STATES	currentState;
			sl12::u32				lastPassNo;
		};	// struct ResourceWork

		// リソースの履歴情報を生成する
		void MakeResourceHistory(std::vector<ResourceProducerBase*>& producers, std::map<ResourceID, RWHistory>& history_map)
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

					auto findIt = history_map.find(id);
					if (findIt == history_map.end())
					{
						// 新規リソースをWrite状態で追加
						RWHistory h(prodCnt);
						h.history.push_back(RWIndex(false, passNo));
						history_map[id] = h;
					}
					else
					{
						// 前回状態がReadならWrite状態を追加する
						auto last_state = findIt->second.history.rbegin();
						if (last_state->isRead)
						{
							findIt->second.history.push_back(RWIndex(false, passNo));
						}
					}
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

					auto findIt = history_map.find(id);
					if (findIt == history_map.end())
					{
						// TODO: この場合はヒストリーバッファとなるが、現在は対応していない
					}
					else
					{
						auto last_state = findIt->second.history.rbegin();
						if (last_state->isRead)
						{
							// 前回状態がReadならReadの最終パスを更新する
							last_state->producerNo = passNo;
						}
						else if (last_state->producerNo == passNo)
						{
							// TODO: この場合はヒストリーバッファを使用することになるが、現在は対応していない
						}
						else
						{
							// 前回状態がWriteならRead状態を追加する
							findIt->second.history.push_back(RWIndex(true, passNo));
						}
					}
				}

				// 一時リソースに自動的にIDを割り当てる
				cnt = prod->GetTempCount();
				for (sl12::u16 i = 0; i < cnt; ++i)
				{
					prod->SetTempID(i, ResourceID::CreateTemporalID((sl12::u8)passNo, (sl12::u8)i));
				}

				prev_prod = prod;
				passNo++;
			}
		}

		// 未使用リソースから使えるものを探す
		std::vector<ResourceWork>::iterator FindFromUnusedResource(std::vector<ResourceWork>& unusedRes, const RenderResourceDesc& desc)
		{
			for (auto it = unusedRes.begin(); it != unusedRes.end(); it++)
			{
				if (it->p_res->IsSameDesc(desc))
				{
					return it;
				}
			}
			return unusedRes.end();
		}

		// リソース生成
		void MakeResourcesDetail(
			sl12::Device& device,
			sl12::u32 screenWidth, sl12::u32 screenHeight,
			const std::map<ResourceID, RWHistory> history_map,
			std::vector<ResourceProducerBase*>& producers,
			std::vector<RenderResource*>& outResources,
			std::map<ResourceID, RenderResource*>& outMap)
		{
			sl12::u16 passNo = 0;
			std::map<ResourceID, ResourceWork> usedRes;
			std::vector<ResourceWork> unusedRes;
			for (auto&& prod : producers)
			{
				// 入力リソースの状態遷移
				auto cnt = prod->GetInputCount();
				auto ids = prod->GetInputIds();
				for (sl12::u32 i = 0; i < cnt; i++)
				{
					auto findIt = usedRes.find(ids[i]);
					assert(findIt != usedRes.end());		// 使用中リソースに必ず存在するはず
					prod->SetInputPrevState(i, findIt->second.currentState);
					findIt->second.currentState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				}

				// 出力リソースを生成する
				cnt = prod->GetOutputCount();
				ids = prod->GetOutputIds();
				auto descs = prod->GetOutputDescs();
				for (sl12::u32 i = 0; i < cnt; i++)
				{
					auto id = ids[i];
					auto findIt = usedRes.find(id);
					if (findIt != usedRes.end())
					{
						// すでにこのリソースが生成されている場合
						prod->SetOutputPrevState(i, findIt->second.currentState);
						if (!id.isSwapchain)
							findIt->second.currentState = findIt->second.p_res->IsRtv() ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE;
						else
							findIt->second.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
					}
					else if (id.isSwapchain)
					{
						// スワップチェインを使用するパスが初めて登場したので、前回状態はPresentとする
						prod->SetOutputPrevState(i, D3D12_RESOURCE_STATE_PRESENT);

						ResourceWork work;
						work.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
						work.lastPassNo = history_map.find(id)->second.history.rbegin()->producerNo;
						usedRes[id] = work;
					}
					else
					{
						ResourceWork work;

						// 未使用リソースに使えるものがあるかチェック
						auto unusedIt = FindFromUnusedResource(unusedRes, descs[i]);
						if (unusedIt != unusedRes.end())
						{
							// 未使用リソースが見つかったので利用する
							work = *unusedIt;
							unusedRes.erase(unusedIt);
							prod->SetOutputPrevState(i, work.currentState);
						}
						else
						{
							// 未使用リソースが存在しないので新規作成
							work.p_res = new RenderResource();
							work.p_res->Initialize(device, descs[i], screenWidth, screenHeight);
							outResources.push_back(work.p_res);
							prod->SetOutputPrevState(i, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
						}

						// ワークを使用マップに登録する
						work.currentState = work.p_res->IsRtv() ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE;
						work.lastPassNo = history_map.find(id)->second.history.rbegin()->producerNo;
						usedRes[id] = work;
						outMap[id] = work.p_res;
					}
				}

				// 一時リソースを生成する
				cnt = prod->GetTempCount();
				ids = prod->GetTempIds();
				descs = prod->GetTempDescs();
				for (sl12::u32 i = 0; i < cnt; i++)
				{
					ResourceWork work;

					auto id = ids[i];
					auto unusedIt = FindFromUnusedResource(unusedRes, descs[i]);
					if (unusedIt != unusedRes.end())
					{
						// 未使用リソースが見つかったので利用する
						work = *unusedIt;
						unusedRes.erase(unusedIt);
						prod->SetTempPrevState(i, work.currentState);
					}
					else
					{
						// 未使用リソースが存在しないので新規作成
						work.p_res = new RenderResource();
						work.p_res->Initialize(device, descs[i], screenWidth, screenHeight);
						outResources.push_back(work.p_res);
						prod->SetTempPrevState(i, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					}

					// ワークを使用マップに登録する
					work.currentState = work.p_res->IsRtv() ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE;
					work.lastPassNo = passNo;
					usedRes[id] = work;
					outMap[id] = work.p_res;
				}

				// 使用中リストを整理する
				auto usedIt = usedRes.begin();
				while (usedIt != usedRes.end())
				{
					if (usedIt->second.lastPassNo <= passNo)
					{
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

		AllReset();

		// 各リソースのR/Wタイミングを記録する
		std::map<ResourceID, RWHistory> history_map;
		MakeResourceHistory(producers, history_map);

		// 実際のリソース生成
		MakeResourcesDetail(*pDevice_, screenWidth_, screenHeight_, history_map, producers, resources_, resource_map_);
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
