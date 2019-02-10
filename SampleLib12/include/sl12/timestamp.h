#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;

	class Timestamp
	{
		friend class CommandList;

	public:
		Timestamp()
		{}
		~Timestamp()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, size_t count);
		void Destroy();

		void Reset();
		void Query(CommandList* pCmdList);
		void Resolve(CommandList* pCmdList);

		size_t GetTimestamp(size_t start_index, size_t count, uint64_t* pOut);

	private:
		ID3D12QueryHeap*		pQuery_ = nullptr;
		ID3D12Resource*			pResource_ = nullptr;
		size_t					maxCount_ = 0;
		size_t					currentCount_ = 0;
	};	// class Timestamp

}	// namespace sl12

//	EOF
