#include <sl12/command_list.h>

#include <sl12/device.h>
#include <sl12/command_queue.h>

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

}	// namespace sl12

//	EOF
