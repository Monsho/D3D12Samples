#include <sl12/timestamp.h>

#include <sl12/device.h>
#include <sl12/command_list.h>
#include <sl12/fence.h>


namespace sl12
{
	//----
	bool Timestamp::Initialize(Device* pDev, size_t count)
	{
		D3D12_QUERY_HEAP_DESC qd{};
		qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		qd.Count = (UINT)count;
		qd.NodeMask = 1;

		auto hr = pDev->GetDeviceDep()->CreateQueryHeap(&qd, IID_PPV_ARGS(&pQuery_));
		if (FAILED(hr))
		{
			return false;
		}

		D3D12_HEAP_PROPERTIES prop{};
		prop.Type = D3D12_HEAP_TYPE_READBACK;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC rd{};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Alignment = 0;
		rd.Width = sizeof(uint64_t) * qd.Count;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc.Count = 1;
		rd.SampleDesc.Quality = 0;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rd.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		hr = pDev->GetDeviceDep()->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pResource_));
		if (FAILED(hr))
		{
			return false;
		}

		currentCount_ = 0;
		maxCount_ = count;
		return true;
	}

	//----
	void Timestamp::Destroy()
	{
		SafeRelease(pResource_);
		SafeRelease(pQuery_);
	}

	//----
	void Timestamp::Reset()
	{
		currentCount_ = 0;
	}

	//----
	void Timestamp::Query(CommandList* pCmdList)
	{
		pCmdList->GetCommandList()->EndQuery(pQuery_, D3D12_QUERY_TYPE_TIMESTAMP, (UINT)currentCount_);
		currentCount_++;
	}

	//----
	void Timestamp::Resolve(CommandList* pCmdList)
	{
		pCmdList->GetCommandList()->ResolveQueryData(pQuery_, D3D12_QUERY_TYPE_TIMESTAMP, 0, (UINT)currentCount_, pResource_, 0);
	}

	//----
	size_t Timestamp::GetTimestamp(size_t start_index, size_t count, uint64_t* pOut)
	{
		if (start_index >= maxCount_)
		{
			return 0;
		}

		auto begin = start_index;
		auto end = start_index + count;
		if (end > maxCount_)
		{
			end = maxCount_;
		}

		D3D12_RANGE range{ sizeof(uint64_t) * begin, sizeof(uint64_t) * end };
		void* p;
		pResource_->Map(0, &range, &p);
		memcpy(pOut, p, sizeof(uint64_t) * (end - begin));
		pResource_->Unmap(0, nullptr);

		return end - begin;
	}

}	// namespace sl12

//	EOF
