#include <sl12/command_list.h>

#include <sl12/device.h>
#include <sl12/command_queue.h>
#include <sl12/texture.h>
#include <sl12/buffer.h>

namespace sl12
{
	//----
	bool CommandList::Initialize(Device* pDev, CommandQueue* pQueue)
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

		pCmdList_->Close();

		pParentQueue_ = pQueue;
		return true;
	}

	//----
	void CommandList::Destroy()
	{
		pParentQueue_ = nullptr;
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

}	// namespace sl12

//	EOF
