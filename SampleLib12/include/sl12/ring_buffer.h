#pragma once

#include "sl12/types.h"

#include <atomic>
#include <mutex>


namespace sl12
{
	class Device;
	class Buffer;
	class CommandList;

	//----------------
	// Ring buffer for copy.
	class CopyRingBuffer
	{
	public:
		struct Result
		{
			Buffer*		pBuffer;
			u32			offset;
			u32			size;
		};	// struct Result

	public:
		CopyRingBuffer(Device* pDev);
		~CopyRingBuffer();

		// need to call when frame begin.
		void BeginNewFrame();

		// only copy data to ring buffer.
		Result CopyToRing(const void* pData, u32 size);

		// copy data to ring buffer and load dma copy command.
		void CopyToBuffer(CommandList* pCmdList, Buffer* pDstBuffer, u32 dstOffset, const void* pData, u32 size);

	private:
		Device*		pParentDevice_ = nullptr;
		Buffer*		pCopySource_ = nullptr;

		std::mutex	mutex_;
		u32			size_;
		u32			head_;
		u32			tail_;
		u32			prevHead_;
	};	// class CopyRingBuffer

}	// namespace sl12

//	EOF
