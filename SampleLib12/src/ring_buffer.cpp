#include <sl12/ring_buffer.h>

#include <sl12/device.h>
#include <sl12/command_list.h>
#include <sl12/buffer.h>


namespace sl12
{
	//----
	CopyRingBuffer::CopyRingBuffer(Device* pDev)
		: pParentDevice_(pDev)
	{
		head_ = tail_ = prevHead_ = 0;
		size_ = 64 * 1024;		// init 64KB

		pCopySource_ = new Buffer();
		bool bSuccess = pCopySource_->Initialize(pParentDevice_, size_, 0, sl12::BufferUsage::ConstantBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true, false);
		assert(bSuccess);
	}

	//----
	CopyRingBuffer::~CopyRingBuffer()
	{
		if (pCopySource_)
		{
			pParentDevice_->KillObject(pCopySource_);
			pCopySource_ = nullptr;
		}
	}

	//----
	// need to call when frame begin.
	void CopyRingBuffer::BeginNewFrame()
	{
		prevHead_ = head_;
		head_ = tail_;
	}

	//----
	// only copy data to ring buffer.
	CopyRingBuffer::Result CopyRingBuffer::CopyToRing(const void* pData, u32 size)
	{
		static const u32 kWaterMark = 16;
		Result result;

		std::unique_lock<std::mutex> lock(mutex_);
		if ((tail_ > head_) && (size > size_ - tail_))
		{
			tail_ = 0;
		}
		if ((tail_ < head_) && (head_ < tail_ + size + kWaterMark))
		{
			// create new ring buffer.
			pParentDevice_->KillObject(pCopySource_);
			pCopySource_ = new Buffer();
			do
			{
				size_ *= 2;
			} while(size_ < size);
			bool bSuccess = pCopySource_->Initialize(pParentDevice_, size_, 0, sl12::BufferUsage::ConstantBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true, false);
			assert(bSuccess);

			prevHead_ = head_ = tail_ = 0;
		}

		result.pBuffer = pCopySource_;
		result.offset = tail_;
		result.size = size;

		D3D12_RANGE range;
		range.Begin = (SIZE_T)result.offset;
		range.End = (SIZE_T)(result.offset + result.size);
		void* p = pCopySource_->Map(range);
		assert(p != nullptr);
		memcpy(p, pData, size);
		pCopySource_->Unmap(range);

		tail_ += size;

		return result;
	}

	//----
	// copy data to ring buffer and load dma copy command.
	void CopyRingBuffer::CopyToBuffer(CommandList* pCmdList, Buffer* pDstBuffer, u32 dstOffset, const void* pData, u32 size)
	{
		auto result = CopyToRing(pData, size);

		pCmdList->GetLatestCommandList()->CopyBufferRegion(pDstBuffer->GetResourceDep(), dstOffset, result.pBuffer->GetResourceDep(), result.offset, result.size);
	}

}	// namespace sl12

//	EOF
