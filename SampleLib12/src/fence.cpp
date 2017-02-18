#include <sl12/fence.h>

#include <sl12/device.h>
#include <sl12/command_list.h>
#include <sl12/command_queue.h>


namespace sl12
{
	//----
	bool Fence::Initialize(Device* pDev)
	{
		auto hr = pDev->GetDeviceDep()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence_));
		if (FAILED(hr))
		{
			return false;
		}
		value_ = 1;

		hEvent_ = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		if (hEvent_ == nullptr)
		{
			return false;
		}

		return true;
	}

	//----
	void Fence::Destroy()
	{
		//CloseHandle(hEvent_);
		SafeRelease(pFence_);
	}

	//----
	void Fence::Signal(CommandQueue* pQueue)
	{
		waitValue_ = value_;
		pQueue->GetQueueDep()->Signal(pFence_, waitValue_);
		value_++;
	}

	//----
	void Fence::Signal(CommandQueue* pQueue, u32 value)
	{
		pQueue->GetQueueDep()->Signal(pFence_, value);
	}

	//----
	void Fence::WaitSignal()
	{
		if (pFence_->GetCompletedValue() < waitValue_)
		{
			pFence_->SetEventOnCompletion(waitValue_, hEvent_);
			WaitForSingleObject(hEvent_, INFINITE);
		}
	}

	//----
	void Fence::WaitSignal(u32 value)
	{
		if (pFence_->GetCompletedValue() < value)
		{
			pFence_->SetEventOnCompletion(value, hEvent_);
			WaitForSingleObject(hEvent_, INFINITE);
		}
	}

	//----
	void Fence::WaitSignal(CommandQueue* pQueue)
	{
		pQueue->GetQueueDep()->Wait(pFence_, waitValue_);
	}

	//----
	bool Fence::CheckSignal()
	{
		u64 completedValue = pFence_->GetCompletedValue();
		return !(completedValue < waitValue_);
	}

}	// namespace sl12

//	EOF
