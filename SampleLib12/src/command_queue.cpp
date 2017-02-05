#include <sl12/command_queue.h>

#include <sl12/device.h>


namespace sl12
{
	//----
	bool CommandQueue::Initialize(Device* pDev, D3D12_COMMAND_LIST_TYPE type)
	{
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = type;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;		// GPUタイムアウトが有効
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		auto hr = pDev->GetDeviceDep()->CreateCommandQueue(&desc, IID_PPV_ARGS(&pQueue_));
		if (FAILED(hr))
		{
			return false;
		}

		listType_ = type;
		return true;
	}

	//----
	void CommandQueue::Destroy()
	{
		SafeRelease(pQueue_);
	}
}	// namespace sl12

//	EOF
