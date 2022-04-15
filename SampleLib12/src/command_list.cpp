#include <sl12/command_list.h>

#include <sl12/device.h>
#include <sl12/command_queue.h>
#include <sl12/texture.h>
#include <sl12/buffer.h>
#include <sl12/descriptor_heap.h>
#include <sl12/descriptor_set.h>
#include <sl12/root_signature.h>

#define USE_PIX 1
#include <WinPixEventRuntime/pix3.h>


namespace sl12
{
	//----
	bool CommandList::Initialize(Device* pDev, CommandQueue* pQueue, bool forDxr)
	{
		auto hr = pDev->GetDeviceDep()->CreateCommandAllocator(pQueue->listType_, IID_PPV_ARGS(&pCmdAllocator_));
		if (FAILED(hr))
		{
			return false;
		}

		hr = pDev->GetDeviceDep()->CreateCommandList(0, pQueue->listType_, pCmdAllocator_, nullptr, IID_PPV_ARGS(&pCmdList_));
		if (FAILED(hr))
		{
			return false;
		}

		hr = pCmdList_->QueryInterface(IID_PPV_ARGS(&pLatestCmdList_));
		if (FAILED(hr))
		{
			// todo: error.
		}

		pCmdList_->Close();

		if (pQueue->listType_ == D3D12_COMMAND_LIST_TYPE_DIRECT || pQueue->listType_ == D3D12_COMMAND_LIST_TYPE_COMPUTE)
		{
			pViewDescStack_ = new DescriptorStackList();
			if (!pViewDescStack_->Initialilze(&pDev->GetGlobalViewDescriptorHeap()))
			{
				return false;
			}

			pSamplerDescCache_ = new SamplerDescriptorCache();
			if (!pSamplerDescCache_->Initialize(pDev))
			{
				return false;
			}

			pCurrentSamplerHeap_ = pPrevSamplerHeap_ = nullptr;
		}
		changeHeap_ = true;

		pParentDevice_ = pDev;
		pParentQueue_ = pQueue;
		return true;
	}

	//----
	void CommandList::Destroy()
	{
		pParentQueue_ = nullptr;
		SafeDelete(pSamplerDescCache_);
		SafeDelete(pViewDescStack_);
		SafeRelease(pLatestCmdList_);
		SafeRelease(pCmdList_);
		SafeRelease(pCmdAllocator_);
	}

	//----
	void CommandList::Reset()
	{
		auto hr = pCmdAllocator_->Reset();
		assert(SUCCEEDED(hr));

		hr = pCmdList_->Reset(pCmdAllocator_, nullptr);
		assert(SUCCEEDED(hr));

		if (pViewDescStack_)
			pViewDescStack_->Reset();

		changeHeap_ = true;
	}

	//----
	void CommandList::Close()
	{
		auto hr = pCmdList_->Close();
		assert(SUCCEEDED(hr));
	}

	//----
	void CommandList::Execute()
	{
		ID3D12CommandList* lists[] = { pCmdList_ };
		pParentQueue_->GetQueueDep()->ExecuteCommandLists(ARRAYSIZE(lists), lists);
	}

	//----
	void CommandList::TransitionBarrier(Texture* p, D3D12_RESOURCE_STATES nextState)
	{
		if (!p)
			return;

		if (p->currentState_ != nextState)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = p->pResource_;
			barrier.Transition.StateBefore = p->currentState_;
			barrier.Transition.StateAfter = nextState;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			GetCommandList()->ResourceBarrier(1, &barrier);

			p->currentState_ = nextState;
		}
	}

	//----
	void CommandList::TransitionBarrier(Buffer* p, D3D12_RESOURCE_STATES nextState)
	{
		if (!p)
			return;

		if (p->currentState_ != nextState)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = p->pResource_;
			barrier.Transition.StateBefore = p->currentState_;
			barrier.Transition.StateAfter = nextState;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			GetCommandList()->ResourceBarrier(1, &barrier);

			p->currentState_ = nextState;
		}
	}

	//----
	void CommandList::TransitionBarrier(Texture* p, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState)
	{
		if (!p)
			return;

		if (prevState != nextState)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = p->pResource_;
			barrier.Transition.StateBefore = prevState;
			barrier.Transition.StateAfter = nextState;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			GetCommandList()->ResourceBarrier(1, &barrier);
		}
	}
	void CommandList::TransitionBarrier(Texture* p, UINT subresource, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState)
	{
		if (!p)
			return;

		if (prevState != nextState)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = p->pResource_;
			barrier.Transition.StateBefore = prevState;
			barrier.Transition.StateAfter = nextState;
			barrier.Transition.Subresource = subresource;
			GetCommandList()->ResourceBarrier(1, &barrier);
		}
	}

	//----
	void CommandList::TransitionBarrier(Buffer* p, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState)
	{
		if (!p)
			return;

		if (prevState != nextState)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = p->pResource_;
			barrier.Transition.StateBefore = prevState;
			barrier.Transition.StateAfter = nextState;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			GetCommandList()->ResourceBarrier(1, &barrier);
		}
	}

	//----
	void CommandList::UAVBarrier(Texture* p)
	{
		if (!p)
			return;

		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.UAV.pResource = p->pResource_;
			GetCommandList()->ResourceBarrier(1, &barrier);
		}
	}

	//----
	void CommandList::UAVBarrier(Buffer* p)
	{
		if (!p)
			return;

		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.UAV.pResource = p->pResource_;
			GetCommandList()->ResourceBarrier(1, &barrier);
		}
	}

	//----
	void CommandList::SetGraphicsRootSignatureAndDescriptorSet(RootSignature* pRS, DescriptorSet* pDSet, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>** ppBindlessArrays)
	{
		auto pCmdList = GetCommandList();
		auto def_view = pParentDevice_->GetDefaultViewDescInfo().cpuHandle;
		auto def_sampler = pParentDevice_->GetDefaultSamplerDescInfo().cpuHandle;

		auto&& input_index = pRS->GetInputIndex();
		pCmdList->SetGraphicsRootSignature(pRS->GetRootSignature());

		// サンプラーステートのハンドルを確保する
		D3D12_GPU_DESCRIPTOR_HANDLE samplerGpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE sampler_handles[16 * 5];
		u32 sampler_count = 0;
		auto SetSamplerHandle = [&](const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			sampler_handles[sampler_count++] = (handle.ptr > 0) ? handle : def_sampler;
		};
		for (u32 i = 0; i < pDSet->GetVsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetVsSampler().cpuHandles[i]);
		}
		for (u32 i = 0; i < pDSet->GetPsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetPsSampler().cpuHandles[i]);
		}
		for (u32 i = 0; i < pDSet->GetGsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetGsSampler().cpuHandles[i]);
		}
		for (u32 i = 0; i < pDSet->GetHsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetHsSampler().cpuHandles[i]);
		}
		for (u32 i = 0; i < pDSet->GetDsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetDsSampler().cpuHandles[i]);
		}
		if (sampler_count > 0)
		{
			bool isSuccess = pSamplerDescCache_->AllocateAndCopy(sampler_count, sampler_handles, samplerGpuHandle);
			assert(isSuccess);

			pCurrentSamplerHeap_ = pSamplerDescCache_->GetHeap();
			if (pCurrentSamplerHeap_ != pPrevSamplerHeap_)
			{
				pPrevSamplerHeap_ = pCurrentSamplerHeap_;
				changeHeap_ = true;
			}
		}

		// Heapが変更されている場合はここで設定し直す
		if (changeHeap_)
		{
			ID3D12DescriptorHeap* pDescHeaps[2];
			int heap_cnt = 0;
			if (pParentDevice_->GetGlobalViewDescriptorHeap().GetHeap())
				pDescHeaps[heap_cnt++] = pParentDevice_->GetGlobalViewDescriptorHeap().GetHeap();
			if (pCurrentSamplerHeap_)
				pDescHeaps[heap_cnt++] = pCurrentSamplerHeap_;
			pCmdList->SetDescriptorHeaps(heap_cnt, pDescHeaps);
			changeHeap_ = false;
		}

		if (sampler_count > 0)
		{
			// サンプラーステートのハンドルを登録する
			auto sampler_desc_size = pParentDevice_->GetDeviceDep()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			auto SetSamplerDesc = [&](u32 count, u8 index)
			{
				if (count > 0)
				{
					pCmdList->SetGraphicsRootDescriptorTable(index, samplerGpuHandle);
					samplerGpuHandle.ptr += sampler_desc_size * count;
				}
			};
			SetSamplerDesc(pDSet->GetVsSampler().maxCount, input_index.vsSamplerIndex_);
			SetSamplerDesc(pDSet->GetPsSampler().maxCount, input_index.psSamplerIndex_);
			SetSamplerDesc(pDSet->GetGsSampler().maxCount, input_index.gsSamplerIndex_);
			SetSamplerDesc(pDSet->GetHsSampler().maxCount, input_index.hsSamplerIndex_);
			SetSamplerDesc(pDSet->GetDsSampler().maxCount, input_index.dsSamplerIndex_);
		}

		// CBV, SRV, UAVの登録
		auto SetViewDesc = [&](u32 count, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, u8 index)
		{
			if (count > 0)
			{
				std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> tmp;
				tmp.resize(count);
				for (u32 i = 0; i < count; i++)
				{
					tmp[i] = (handles[i].ptr > 0) ? handles[i] : def_view;
				}

				D3D12_CPU_DESCRIPTOR_HANDLE dst_cpu;
				D3D12_GPU_DESCRIPTOR_HANDLE dst_gpu;
				pViewDescStack_->Allocate(count, dst_cpu, dst_gpu);
				pParentDevice_->GetDeviceDep()->CopyDescriptors(
					1, &dst_cpu, &count,
					count, tmp.data(), nullptr,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				pCmdList->SetGraphicsRootDescriptorTable(index, dst_gpu);
			}
		};
		SetViewDesc(pDSet->GetVsCbv().maxCount, pDSet->GetVsCbv().cpuHandles, input_index.vsCbvIndex_);
		SetViewDesc(pDSet->GetVsSrv().maxCount, pDSet->GetVsSrv().cpuHandles, input_index.vsSrvIndex_);
		SetViewDesc(pDSet->GetPsCbv().maxCount, pDSet->GetPsCbv().cpuHandles, input_index.psCbvIndex_);
		SetViewDesc(pDSet->GetPsSrv().maxCount, pDSet->GetPsSrv().cpuHandles, input_index.psSrvIndex_);
		SetViewDesc(pDSet->GetPsUav().maxCount, pDSet->GetPsUav().cpuHandles, input_index.psUavIndex_);
		SetViewDesc(pDSet->GetGsCbv().maxCount, pDSet->GetGsCbv().cpuHandles, input_index.gsCbvIndex_);
		SetViewDesc(pDSet->GetGsSrv().maxCount, pDSet->GetGsSrv().cpuHandles, input_index.gsSrvIndex_);
		SetViewDesc(pDSet->GetHsCbv().maxCount, pDSet->GetHsCbv().cpuHandles, input_index.hsCbvIndex_);
		SetViewDesc(pDSet->GetHsSrv().maxCount, pDSet->GetHsSrv().cpuHandles, input_index.hsSrvIndex_);
		SetViewDesc(pDSet->GetDsCbv().maxCount, pDSet->GetDsCbv().cpuHandles, input_index.dsCbvIndex_);
		SetViewDesc(pDSet->GetDsSrv().maxCount, pDSet->GetDsSrv().cpuHandles, input_index.dsSrvIndex_);

		// set bindless srv.
		auto bindless_infos = pRS->GetBindlessInfos();
		if (ppBindlessArrays && bindless_infos.size() != 0)
		{
			auto bindless_array_count = bindless_infos.size();
			for (size_t ba = 0; ba < bindless_array_count; ba++)
			{
				if (ppBindlessArrays[ba])
				{
					auto&& bindless = *ppBindlessArrays[ba];
					SetViewDesc((u32)bindless.size(), bindless.data(), bindless_infos[ba].index_);
				}
			}
		}
	}

	//----
	void CommandList::SetMeshRootSignatureAndDescriptorSet(RootSignature* pRS, DescriptorSet* pDSet, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>** ppBindlessArrays)
	{
		auto pCmdList = GetCommandList();
		auto def_view = pParentDevice_->GetDefaultViewDescInfo().cpuHandle;
		auto def_sampler = pParentDevice_->GetDefaultSamplerDescInfo().cpuHandle;

		auto&& input_index = pRS->GetInputIndex();
		pCmdList->SetGraphicsRootSignature(pRS->GetRootSignature());

		// サンプラーステートのハンドルを確保する
		D3D12_GPU_DESCRIPTOR_HANDLE samplerGpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE sampler_handles[16 * 3];
		u32 sampler_count = 0;
		auto SetSamplerHandle = [&](const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			sampler_handles[sampler_count++] = (handle.ptr > 0) ? handle : def_sampler;
		};
		for (u32 i = 0; i < pDSet->GetAsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetAsSampler().cpuHandles[i]);
		}
		for (u32 i = 0; i < pDSet->GetMsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetMsSampler().cpuHandles[i]);
		}
		for (u32 i = 0; i < pDSet->GetPsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetPsSampler().cpuHandles[i]);
		}
		if (sampler_count > 0)
		{
			bool isSuccess = pSamplerDescCache_->AllocateAndCopy(sampler_count, sampler_handles, samplerGpuHandle);
			assert(isSuccess);

			pCurrentSamplerHeap_ = pSamplerDescCache_->GetHeap();
			if (pCurrentSamplerHeap_ != pPrevSamplerHeap_)
			{
				pPrevSamplerHeap_ = pCurrentSamplerHeap_;
				changeHeap_ = true;
			}
		}

		// Heapが変更されている場合はここで設定し直す
		if (changeHeap_)
		{
			ID3D12DescriptorHeap* pDescHeaps[2];
			int heap_cnt = 0;
			if (pParentDevice_->GetGlobalViewDescriptorHeap().GetHeap())
				pDescHeaps[heap_cnt++] = pParentDevice_->GetGlobalViewDescriptorHeap().GetHeap();
			if (pCurrentSamplerHeap_)
				pDescHeaps[heap_cnt++] = pCurrentSamplerHeap_;
			pCmdList->SetDescriptorHeaps(heap_cnt, pDescHeaps);
			changeHeap_ = false;
		}

		if (sampler_count > 0)
		{
			// サンプラーステートのハンドルを登録する
			auto sampler_desc_size = pParentDevice_->GetDeviceDep()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			auto SetSamplerDesc = [&](u32 count, u8 index)
			{
				if (count > 0)
				{
					pCmdList->SetGraphicsRootDescriptorTable(index, samplerGpuHandle);
					samplerGpuHandle.ptr += sampler_desc_size * count;
				}
			};
			SetSamplerDesc(pDSet->GetAsSampler().maxCount, input_index.asSamplerIndex_);
			SetSamplerDesc(pDSet->GetMsSampler().maxCount, input_index.msSamplerIndex_);
			SetSamplerDesc(pDSet->GetPsSampler().maxCount, input_index.psSamplerIndex_);
		}

		// CBV, SRV, UAVの登録
		auto SetViewDesc = [&](u32 count, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, u8 index)
		{
			if (count > 0)
			{
				std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> tmp;
				tmp.resize(count);
				for (u32 i = 0; i < count; i++)
				{
					tmp[i] = (handles[i].ptr > 0) ? handles[i] : def_view;
				}

				D3D12_CPU_DESCRIPTOR_HANDLE dst_cpu;
				D3D12_GPU_DESCRIPTOR_HANDLE dst_gpu;
				pViewDescStack_->Allocate(count, dst_cpu, dst_gpu);
				pParentDevice_->GetDeviceDep()->CopyDescriptors(
					1, &dst_cpu, &count,
					count, tmp.data(), nullptr,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				pCmdList->SetGraphicsRootDescriptorTable(index, dst_gpu);
			}
		};
		SetViewDesc(pDSet->GetAsCbv().maxCount, pDSet->GetAsCbv().cpuHandles, input_index.asCbvIndex_);
		SetViewDesc(pDSet->GetAsSrv().maxCount, pDSet->GetAsSrv().cpuHandles, input_index.asSrvIndex_);
		SetViewDesc(pDSet->GetMsCbv().maxCount, pDSet->GetMsCbv().cpuHandles, input_index.msCbvIndex_);
		SetViewDesc(pDSet->GetMsSrv().maxCount, pDSet->GetMsSrv().cpuHandles, input_index.msSrvIndex_);
		SetViewDesc(pDSet->GetPsCbv().maxCount, pDSet->GetPsCbv().cpuHandles, input_index.psCbvIndex_);
		SetViewDesc(pDSet->GetPsSrv().maxCount, pDSet->GetPsSrv().cpuHandles, input_index.psSrvIndex_);
		SetViewDesc(pDSet->GetPsUav().maxCount, pDSet->GetPsUav().cpuHandles, input_index.psUavIndex_);

		// set bindless srv.
		auto bindless_infos = pRS->GetBindlessInfos();
		if (ppBindlessArrays && bindless_infos.size() != 0)
		{
			auto bindless_array_count = bindless_infos.size();
			for (size_t ba = 0; ba < bindless_array_count; ba++)
			{
				if (ppBindlessArrays[ba])
				{
					auto&& bindless = *ppBindlessArrays[ba];
					SetViewDesc((u32)bindless.size(), bindless.data(), bindless_infos[ba].index_);
				}
			}
		}
	}

	//----
	void CommandList::SetComputeRootSignatureAndDescriptorSet(RootSignature* pRS, DescriptorSet* pDSet, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>** ppBindlessArrays)
	{
		auto pCmdList = GetCommandList();
		auto def_view = pParentDevice_->GetDefaultViewDescInfo().cpuHandle;
		auto def_sampler = pParentDevice_->GetDefaultSamplerDescInfo().cpuHandle;

		auto&& input_index = pRS->GetInputIndex();
		pCmdList->SetComputeRootSignature(pRS->GetRootSignature());

		D3D12_GPU_DESCRIPTOR_HANDLE samplerGpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE sampler_handles[16];
		u32 sampler_count = 0;
		auto SetSamplerHandle = [&](const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			sampler_handles[sampler_count++] = (handle.ptr > 0) ? handle : def_sampler;
		};
		for (u32 i = 0; i < pDSet->GetCsSampler().maxCount; i++)
		{
			SetSamplerHandle(pDSet->GetCsSampler().cpuHandles[i]);
		}
		if (sampler_count > 0)
		{
			bool isSuccess = pSamplerDescCache_->AllocateAndCopy(sampler_count, sampler_handles, samplerGpuHandle);
			assert(isSuccess);

			pCurrentSamplerHeap_ = pSamplerDescCache_->GetHeap();
			if (pCurrentSamplerHeap_ != pPrevSamplerHeap_)
			{
				pPrevSamplerHeap_ = pCurrentSamplerHeap_;
				changeHeap_ = true;
			}
		}

		if (changeHeap_)
		{
			ID3D12DescriptorHeap* pDescHeaps[2];
			int heap_cnt = 0;
			if (pParentDevice_->GetGlobalViewDescriptorHeap().GetHeap())
				pDescHeaps[heap_cnt++] = pParentDevice_->GetGlobalViewDescriptorHeap().GetHeap();
			if (pCurrentSamplerHeap_)
				pDescHeaps[heap_cnt++] = pCurrentSamplerHeap_;
			pCmdList->SetDescriptorHeaps(heap_cnt, pDescHeaps);
			changeHeap_ = false;
		}

		if (sampler_count > 0)
		{
			// サンプラーステートのハンドルを登録する
			auto sampler_desc_size = pParentDevice_->GetDeviceDep()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			auto SetSamplerDesc = [&](u32 count, u8 index)
			{
				if (count > 0)
				{
					pCmdList->SetComputeRootDescriptorTable(index, samplerGpuHandle);
					samplerGpuHandle.ptr += sampler_desc_size * count;
				}
			};
			SetSamplerDesc(pDSet->GetCsSampler().maxCount, input_index.csSamplerIndex_);
		}

		// CBV, SRV, UAVの登録
		auto SetViewDesc = [&](u32 count, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, u8 index)
		{
			if (count > 0)
			{
				std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> tmp;
				tmp.resize(count);
				for (u32 i = 0; i < count; i++)
				{
					tmp[i] = (handles[i].ptr > 0) ? handles[i] : def_view;
				}

				D3D12_CPU_DESCRIPTOR_HANDLE dst_cpu;
				D3D12_GPU_DESCRIPTOR_HANDLE dst_gpu;
				pViewDescStack_->Allocate(count, dst_cpu, dst_gpu);
				pParentDevice_->GetDeviceDep()->CopyDescriptors(
					1, &dst_cpu, &count,
					count, tmp.data(), nullptr,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				pCmdList->SetComputeRootDescriptorTable(index, dst_gpu);
			}
		};
		SetViewDesc(pDSet->GetCsCbv().maxCount, pDSet->GetCsCbv().cpuHandles, input_index.csCbvIndex_);
		SetViewDesc(pDSet->GetCsSrv().maxCount, pDSet->GetCsSrv().cpuHandles, input_index.csSrvIndex_);
		SetViewDesc(pDSet->GetCsUav().maxCount, pDSet->GetCsUav().cpuHandles, input_index.csUavIndex_);

		// set bindless srv.
		auto bindless_infos = pRS->GetBindlessInfos();
		if (ppBindlessArrays && bindless_infos.size() != 0)
		{
			auto bindless_array_count = bindless_infos.size();
			for (size_t ba = 0; ba < bindless_array_count; ba++)
			{
				if (ppBindlessArrays[ba])
				{
					auto&& bindless = *ppBindlessArrays[ba];
					SetViewDesc((u32)bindless.size(), bindless.data(), bindless_infos[ba].index_);
				}
			}
		}
	}

	//----
	void CommandList::SetRaytracingGlobalRootSignatureAndDescriptorSet(
		RootSignature* pRS,
		DescriptorSet* pDSet,
		RaytracingDescriptorManager* pRtDescMan,
		D3D12_GPU_VIRTUAL_ADDRESS* asAddress,
		u32 asAddressCount)
	{
		auto pCmdList = GetCommandList();
		auto def_view = pParentDevice_->GetDefaultViewDescInfo().cpuHandle;
		auto def_sampler = pParentDevice_->GetDefaultSamplerDescInfo().cpuHandle;

		// グローバルルートシグネチャを設定
		pCmdList->SetComputeRootSignature(pRS->GetRootSignature());

		// デスクリプタヒープを設定
		pRtDescMan->SetHeapToCommandList(*this);

		// Global用のハンドルを取得する
		auto global_handle_start = pRtDescMan->IncrementGlobalHandleStart();

		// CBV, SRV, UAVの登録
		D3D12_CPU_DESCRIPTOR_HANDLE tmp[kSrvMax];
		u32 slot_index = 0;
		auto SetViewDesc = [&](u32 count, u32 offset, const D3D12_CPU_DESCRIPTOR_HANDLE* handles)
		{
			//if (count > offset)
			if (count > 0)
			{
				auto cnt = count;
				//auto cnt = count - offset;
				for (u32 i = 0; i < cnt; i++)
				{
					tmp[i] = (handles[i + offset].ptr > 0) ? handles[i + offset] : def_view;
				}

				pParentDevice_->GetDeviceDep()->CopyDescriptors(
					1, &global_handle_start.viewCpuHandle, &cnt,
					cnt, tmp, nullptr,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				pCmdList->SetComputeRootDescriptorTable(slot_index++, global_handle_start.viewGpuHandle);

				global_handle_start.viewCpuHandle.ptr += pRtDescMan->GetViewDescSize() * cnt;
				global_handle_start.viewGpuHandle.ptr += pRtDescMan->GetViewDescSize() * cnt;
			}
		};
		SetViewDesc(pRtDescMan->GetGlobalCbvCount(), 0, pDSet->GetCsCbv().cpuHandles);
		SetViewDesc(pRtDescMan->GetGlobalSrvCount(), pRtDescMan->GetASCount(), pDSet->GetCsSrv().cpuHandles);
		SetViewDesc(pRtDescMan->GetGlobalUavCount(), 0, pDSet->GetCsUav().cpuHandles);

		// Samplerの登録
		if (pRtDescMan->GetGlobalSamplerCount() > 0)
		{
			auto cnt = pRtDescMan->GetGlobalSamplerCount();
			auto handles = pDSet->GetCsSampler().cpuHandles;
			for (u32 i = 0; i < cnt; i++)
			{
				tmp[i] = (handles[i].ptr > 0) ? handles[i] : def_sampler;
			}

			pParentDevice_->GetDeviceDep()->CopyDescriptors(
				1, &global_handle_start.samplerCpuHandle, &cnt,
				cnt, tmp, nullptr,
				D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			pCmdList->SetComputeRootDescriptorTable(slot_index++, global_handle_start.samplerGpuHandle);

			global_handle_start.samplerCpuHandle.ptr += pRtDescMan->GetSamplerDescSize() * cnt;
			global_handle_start.samplerGpuHandle.ptr += pRtDescMan->GetSamplerDescSize() * cnt;
		}

		// ASの登録
		for (u32 i = 0; i < asAddressCount; i++)
		{
			pCmdList->SetComputeRootShaderResourceView(slot_index++, asAddress[i]);
		}

		// このコマンドリストが持っているDescriptorHeapをDirtyにしておく
		SetDescriptorHeapDirty();
	}

	//----
	void CommandList::PushMarker(u8 colorIndex, char const* format, ...)
	{
		assert(pLatestCmdList_ != nullptr);

		va_list args;
		va_start(args, format);
		PIXBeginEvent(pLatestCmdList_, PIX_COLOR_INDEX(colorIndex), format, args);
		va_end(args);
	}

	//----
	void CommandList::PopMarker()
	{
		PIXEndEvent(pLatestCmdList_);
	}

}	// namespace sl12

//	EOF
