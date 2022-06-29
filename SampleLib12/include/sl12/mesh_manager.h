#pragma once

#include <sl12/util.h>
#include <sl12/buffer.h>
#include <vector>
#include <list>
#include <memory>
#include <mutex>


namespace sl12
{
	class Device;
	class CommandList;

	//----
	class BufferHeapAllocator
	{
		friend class MeshManager;

		struct Block
		{
			size_t	offset = 0;
			size_t	size = 0;
			bool	isUsed = false;

			Block()
			{}
			Block(size_t _offset, size_t _size, bool _isUsed)
				: offset(_offset), size(_size), isUsed(_isUsed)
			{}
		};	// struct Block

	public:
		struct Handle
		{
			const BufferHeapAllocator*	heap = nullptr;
			size_t						offset = 0;
			size_t						size = 0;

			bool IsValid() const
			{
				return heap != nullptr;
			}
		};	// struct Handle

	public:
		BufferHeapAllocator(Device* pDev, size_t initSize, size_t align, BufferUsage::Type usage);
		~BufferHeapAllocator();

		Handle Allocate(size_t inSize);
		void Free(Handle handle);

		void BeginNewFrame(CommandList* pCmdList);

		Buffer* GetBuffer()
		{
			return pBuffer_;
		}

	private:
		bool CreateNewBuffer(size_t alignedSize);

	private:
		Device*			pParentDevice_ = nullptr;
		Buffer*			pBuffer_ = nullptr;
		Buffer*			pNextBuffer_ = nullptr;
		size_t			initSize_ = 0;
		size_t			alignment_ = 0;

		std::list<Block>	memoryBlocks_;
	};	// class BufferHeapAllocator

	//----
	class MeshManager
	{
		struct CopySrc
		{
			std::vector<u8>	src;
			size_t			offset;
		};	// struct CopySrc

	public:
		using Handle = BufferHeapAllocator::Handle;

	public:
		MeshManager(Device* pDev, size_t vertexSize, size_t indexSize);
		~MeshManager();

		void BeginNewFrame(CommandList* pCmdList);

		Handle DeployVertexBuffer(const void* pData, size_t size);
		Handle DeployIndexBuffer(const void* pData, size_t size);

		void ReleaseVertexBuffer(Handle handle);
		void ReleaseIndexBuffer(Handle handle);

		static D3D12_VERTEX_BUFFER_VIEW CreateVertexView(const Handle& handle, size_t additionalOffset, size_t size, size_t stride)
		{
			D3D12_VERTEX_BUFFER_VIEW ret;
			ret.BufferLocation = handle.heap->pBuffer_->GetResourceDep()->GetGPUVirtualAddress() + handle.offset + additionalOffset;
			ret.SizeInBytes = (UINT)size;
			ret.StrideInBytes = (UINT)stride;
			return ret;
		}
		static D3D12_INDEX_BUFFER_VIEW CreateIndexView(const Handle& handle, size_t additionalOffset, size_t size, size_t stride)
		{
			D3D12_INDEX_BUFFER_VIEW ret;
			ret.BufferLocation = handle.heap->pBuffer_->GetResourceDep()->GetGPUVirtualAddress() + handle.offset + additionalOffset;
			ret.SizeInBytes = (UINT)size;
			ret.Format = (stride == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
			return ret;
		}

	private:
		Device*			pParentDevice_ = nullptr;
		std::mutex		mutex_;

		std::unique_ptr<BufferHeapAllocator>	pVertexHeap_;
		std::unique_ptr<BufferHeapAllocator>	pIndexHeap_;

		std::vector<std::unique_ptr<CopySrc>>	vertexSrc_;
		std::vector<std::unique_ptr<CopySrc>>	indexSrc_;
	};	// class MeshManager

}	// namespace sl12

//	EOF
