#include <sl12/mesh_manager.h>

#include <sl12/device.h>
#include <sl12/command_list.h>


namespace sl12
{
	//----------------
	//----
	BufferHeapAllocator::BufferHeapAllocator(Device* pDev, size_t initSize, size_t align, BufferUsage::Type usage)
		: pParentDevice_(pDev)
		, initSize_(initSize)
		, alignment_(align)
	{
		pBuffer_ = new Buffer();
		bool bSuccess = pBuffer_->Initialize(pDev, initSize, 0, usage, D3D12_RESOURCE_STATE_COMMON, false, false);
		assert(bSuccess);

		Block block;
		block.size = initSize;
		memoryBlocks_.push_back(block);
	}

	//----
	BufferHeapAllocator::~BufferHeapAllocator()
	{
		if (pBuffer_)
		{
			pParentDevice_->KillObject(pBuffer_);
		}
	}

	//----
	BufferHeapAllocator::Handle BufferHeapAllocator::Allocate(size_t inSize)
	{
		size_t alignedSize = GetAlignedSize(inSize, alignment_);
		Handle ret;

		// first fit search.
		for (auto it = memoryBlocks_.begin(); it != memoryBlocks_.end(); it++)
		{
			if (!it->isUsed && it->size >= alignedSize)
			{
				// allocate memory block.
				Block newBlock(it->offset, alignedSize, true);
				memoryBlocks_.insert(it, newBlock);
				it->offset += alignedSize;
				it->size -= alignedSize;
				if (it->size == 0)
				{
					memoryBlocks_.erase(it);
				}

				ret.heap = this;
				ret.offset = newBlock.offset;
				ret.size = newBlock.size;
				return ret;
			}
		}

		// create new buffer.
		if (CreateNewBuffer(alignedSize))
		{
			auto lastBlock = memoryBlocks_.end();
			lastBlock--;

			Block newBlock(lastBlock->offset, alignedSize, true);
			lastBlock->offset += alignedSize;
			lastBlock->size -= alignedSize;
			memoryBlocks_.insert(lastBlock, newBlock);
			if (lastBlock->size == 0)
			{
				memoryBlocks_.erase(lastBlock);
			}

			ret.heap = this;
			ret.offset = newBlock.offset;
			ret.size = newBlock.size;
			return ret;
		}

		return ret;
	}

	//----
	void BufferHeapAllocator::Free(Handle handle)
	{
		if (handle.heap != this)
		{
			// error.
			assert(!"[Error] handle.heap is NOT this heap.");
			return;
		}

		auto prevIt = memoryBlocks_.begin();
		for (auto it = memoryBlocks_.begin(); it != memoryBlocks_.end(); it++)
		{
			if (it->isUsed && it->offset == handle.offset)
			{
				// free memory block.
				it->isUsed = false;
				auto nextIt = it;
				nextIt++;
				if (nextIt != memoryBlocks_.end() && !nextIt->isUsed)
				{
					// merge next block.
					it->size += nextIt->size;
					memoryBlocks_.erase(nextIt);
				}
				if (it != prevIt && !prevIt->isUsed)
				{
					// merge prev block.
					prevIt->size += it->size;
					memoryBlocks_.erase(it);

				}
				return;
			}
			prevIt = it;
		}

		// error.
		assert(!"[Error] free block is NOT in this buffer.");
	}

	//----
	bool BufferHeapAllocator::CreateNewBuffer(size_t alignedSize)
	{
		auto incSize = initSize_;
		while (incSize > alignedSize)
		{
			incSize += initSize_;
		}

		auto currSize = (pNextBuffer_ != nullptr) ? pNextBuffer_->GetSize() : pBuffer_->GetSize();
		auto newSize = currSize + incSize;
		if (pNextBuffer_)
		{
			pParentDevice_->KillObject(pNextBuffer_);
		}
		pNextBuffer_ = new Buffer();
		bool bSuccess = pNextBuffer_->Initialize(pParentDevice_, newSize, 0, pBuffer_->GetBufferUsage(), D3D12_RESOURCE_STATE_GENERIC_READ, false, false);
		if (!bSuccess)
		{
			return false;
		}

		auto lastBlock = memoryBlocks_.end();
		lastBlock--;
		if (!lastBlock->isUsed)
		{
			lastBlock->size += incSize;
		}
		else
		{
			memoryBlocks_.push_back(Block(currSize, incSize, false));
		}

		return true;
	}

	//----
	void BufferHeapAllocator::BeginNewFrame(CommandList* pCmdList)
	{
		if (pNextBuffer_)
		{
			auto copySize = pBuffer_->GetSize();
			pCmdList->TransitionBarrier(pNextBuffer_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
			pCmdList->GetLatestCommandList()->CopyBufferRegion(pNextBuffer_->GetResourceDep(), 0, pBuffer_->GetResourceDep(), 0, copySize);

			pParentDevice_->KillObject(pBuffer_);
			pBuffer_ = pNextBuffer_;
			pNextBuffer_ = nullptr;
		}
	}


	//----------------
	//----
	MeshManager::MeshManager(Device* pDev, size_t vertexSize, size_t indexSize)
		: pParentDevice_(pDev)
	{
		pVertexHeap_ = std::make_unique<BufferHeapAllocator>(pDev, vertexSize, 4, BufferUsage::VertexBuffer);
		pIndexHeap_ = std::make_unique<BufferHeapAllocator>(pDev, indexSize, 4, BufferUsage::IndexBuffer);
	}

	//----
	MeshManager::~MeshManager()
	{
		pVertexHeap_.reset(nullptr);
		pIndexHeap_.reset(nullptr);
	}

	//----
	void MeshManager::BeginNewFrame(CommandList* pCmdList)
	{
		auto BeginFunc = [pCmdList, this](BufferHeapAllocator* pHeap, std::vector<std::unique_ptr<CopySrc>>& src)
		{
			bool hasNext = pHeap->pNextBuffer_ != nullptr;
			bool needDeploy = !src.empty();

			if (!hasNext && !needDeploy)
			{
				return;
			}

			pHeap->BeginNewFrame(pCmdList);
			if (!hasNext)
			{
				pCmdList->TransitionBarrier(pHeap->pBuffer_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
			}

			if (needDeploy)
			{
				size_t totalSize = 0;
				for (auto&& v : src)
				{
					totalSize += v->src.size();
				}
				auto pSrcBuffer = new Buffer();
				pSrcBuffer->Initialize(pParentDevice_, totalSize, 0, BufferUsage::VertexBuffer, true, false);
				{
					u8* p = (u8*)pSrcBuffer->Map();
					for (auto&& v : src)
					{
						memcpy(p, v->src.data(), v->src.size());
						p += v->src.size();
					}
					pSrcBuffer->Unmap();
				}

				size_t srcOffset = 0;
				for (auto&& v : src)
				{
					pCmdList->GetLatestCommandList()->CopyBufferRegion(pHeap->pBuffer_->GetResourceDep(), v->offset, pSrcBuffer->GetResourceDep(), srcOffset, v->src.size());
					srcOffset += v->src.size();
				}

				pParentDevice_->KillObject(pSrcBuffer);
				src.clear();
			}

			pCmdList->TransitionBarrier(pHeap->pBuffer_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		};

		std::unique_lock<std::mutex> lock(mutex_);
		BeginFunc(&*pVertexHeap_, vertexSrc_);
		BeginFunc(&*pIndexHeap_, indexSrc_);
	}

	//----
	MeshManager::Handle MeshManager::DeployVertexBuffer(const void* pData, size_t size)
	{
		std::unique_lock<std::mutex> lock(mutex_);

		auto handle = pVertexHeap_->Allocate(size);
		if (handle.IsValid())
		{
			auto src = std::make_unique<CopySrc>();
			src->src.resize(size);
			memcpy(src->src.data(), pData, size);
			src->offset = handle.offset;
			vertexSrc_.push_back(std::move(src));
		}

		return handle;
	}

	//----
	MeshManager::Handle MeshManager::DeployIndexBuffer(const void* pData, size_t size)
	{
		std::unique_lock<std::mutex> lock(mutex_);

		auto handle = pIndexHeap_->Allocate(size);
		if (handle.IsValid())
		{
			auto src = std::make_unique<CopySrc>();
			src->src.resize(size);
			memcpy(src->src.data(), pData, size);
			src->offset = handle.offset;
			indexSrc_.push_back(std::move(src));
		}

		return handle;
	}

	//----
	void MeshManager::ReleaseVertexBuffer(Handle handle)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		pVertexHeap_->Free(handle);
	}

	//----
	void MeshManager::ReleaseIndexBuffer(Handle handle)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		pIndexHeap_->Free(handle);
	}

}	// namespace sl12

//	EOF
