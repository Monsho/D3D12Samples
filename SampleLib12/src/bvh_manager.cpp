#include "sl12/bvh_manager.h"

#include "sl12/command_list.h"
#include "sl12/scene_mesh.h"


namespace sl12
{
	//----------------
	//----
	BvhMemorySuballocator::BvhMemorySuballocator(Device* pDev, size_t NeedSize)
	{
		size_t alloc_size = 4 * 1024 * 1024;	// 4MB
		while (NeedSize > alloc_size)
		{
			alloc_size *= 2;
		}

		auto device_dep = pDev->GetLatestDeviceDep();

		D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

		D3D12_HEAP_PROPERTIES prop{};
		prop.Type = heapType;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = alloc_size;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		auto hr = device_dep->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&pResource_));
		assert(SUCCEEDED(hr));

		totalSize_ = alloc_size;
		totalBlockCount_ = (u32)(alloc_size / kBlockSize);
		headAddress_ = pResource_->GetGPUVirtualAddress();
		unusedChunks_.push_back(Chunk(0, totalBlockCount_));
	}

	//----
	BvhMemorySuballocator::~BvhMemorySuballocator()
	{
		SafeRelease(pResource_);
		unusedChunks_.clear();
	}

	//----
	bool BvhMemorySuballocator::Alloc(size_t size, D3D12_GPU_VIRTUAL_ADDRESS& address)
	{
		u32 block_count = (u32)((size + kBlockSize - 1) / kBlockSize);
		if (block_count > totalBlockCount_)
		{
			return false;
		}

		// find continuous blocks.
		for (auto it = unusedChunks_.begin(); it != unusedChunks_.end(); it++)
		{
			if (it->count >= block_count)
			{
				size_t offset = (size_t)it->head * (size_t)kBlockSize;
				address = headAddress_ + offset;
				it->head += block_count;
				it->count -= block_count;
				if (!it->count)
				{
					unusedChunks_.erase(it);
				}
				return true;
			}
		}

		return false;
	}

	//----
	void BvhMemorySuballocator::Free(D3D12_GPU_VIRTUAL_ADDRESS address, size_t size)
	{
		// insert chunk.
		u32 block_count = (u32)((size + kBlockSize - 1) / kBlockSize);
		size_t offset = address - headAddress_;
		u32 block_head = (u32)(offset / kBlockSize);
		bool is_inserted = false;
		for (auto it = unusedChunks_.begin(); it != unusedChunks_.end(); it++)
		{
			if (it->head > block_head)
			{
				unusedChunks_.insert(it, Chunk(block_head, block_count));
				is_inserted = true;
			}
		}
		if (!is_inserted)
		{
			unusedChunks_.push_back(Chunk(block_head, block_count));
		}

		// joint continuous chunks.
		auto p = unusedChunks_.begin();
		auto c = p;
		c++;
		while (c != unusedChunks_.end())
		{
			if (p->head + p->count == c->head)
			{
				p->count += c->count;
				c = unusedChunks_.erase(c);
			}
			else
			{
				p++; c++;
			}
		}
	}


	//----------------
	//----
	BvhMemoryAllocator::BvhMemoryAllocator(Device* pDev)
		: pDevice_(pDev)
	{}

	//----
	BvhMemoryAllocator::~BvhMemoryAllocator()
	{
		pDevice_ = nullptr;
		suballocators_.clear();
	}

	//----
	BvhMemoryInfo BvhMemoryAllocator::Alloc(size_t size)
	{
		assert(pDevice_ != nullptr);

		// allocate from existed suballocators.
		for (auto it = suballocators_.begin(); it != suballocators_.end(); it++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS address;
			if ((*it)->Alloc(size, address))
			{
				return BvhMemoryInfo(address, size, it->get());
			}
		}

		// allocate new suballocator.
		auto sub = std::make_unique<BvhMemorySuballocator>(pDevice_, size);
		D3D12_GPU_VIRTUAL_ADDRESS address;
		bool success = sub->Alloc(size, address);
		assert(success);

		BvhMemoryInfo ret(address, size, sub.get());
		suballocators_.push_back(std::move(sub));
		return ret;
	}

	//----
	void BvhMemoryAllocator::Free(BvhMemoryInfo& info)
	{
		if (info.pSuballocator_)
		{
			info.pSuballocator_->Free(info.address_, info.size_);
		}
	}


	//----------------
	//----
	BvhGeometry::BvhGeometry()
	{}

	//----
	BvhGeometry::~BvhGeometry()
	{}

	//----
	bool BvhGeometry::Initialize(const ResourceItemMesh* pRes)
	{
		auto res_nc = const_cast<ResourceItemMesh*>(pRes);
		auto&& submeshes = res_nc->GetSubmeshes();
		auto&& materials = res_nc->GetMaterials();
		auto&& vb = res_nc->GetPositionVB();
		auto&& ib = res_nc->GetIndexBuffer();

		geomDescs_.resize(submeshes.size());
		for (int i = 0; i < submeshes.size(); i++)
		{
			auto&& submesh = submeshes[i];
			auto&& desc = geomDescs_[i];
			auto&& material = materials[submesh.materialIndex];

			auto flags = material.isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			desc.Flags = flags;
			desc.Triangles.VertexBuffer.StartAddress = vb.GetResourceDep()->GetGPUVirtualAddress() + submesh.positionVBV.GetBufferOffset();
			desc.Triangles.VertexBuffer.StrideInBytes = vb.GetStride();
			desc.Triangles.VertexCount = submesh.vertexCount;
			desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			desc.Triangles.IndexBuffer = ib.GetResourceDep()->GetGPUVirtualAddress() + submesh.indexBV.GetBufferOffset();
			desc.Triangles.IndexCount = submesh.indexCount;
			desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
			desc.Triangles.Transform3x4 = 0;
		}

		time_ = 0;
		isBuilt_ = false;
		isCompacted = false;
		isCompactEnable = true;

		return true;
	}

	//----
	bool BvhGeometry::IsValid() const
	{
		return isBuilt_;
	}



	//----------------
	//----
	BvhScene::BvhScene()
	{}

	//----
	BvhScene::~BvhScene()
	{}


	//----------------
	//----
	BvhManager::BvhManager(Device* pDev)
		: pDevice_(pDev)
	{
		allocator_ = std::make_unique<BvhMemoryAllocator>(pDev);
	}

	//----
	BvhManager::~BvhManager()
	{
		staticGeomMap_.clear();
		allocator_.reset();

		pDevice_->KillObject(pScratchBuffer_);
		pDevice_->KillObject(pCompactionInfoBuffer_);
	}

	//----
	BvhManager::GeometryKey BvhManager::GetGeometryKey(MeshRenderCommand* pCmd)
	{
		return pCmd->GetParentMesh()->GetParentResource();
	}

	//----
	BvhGeometryWeakPtr BvhManager::AddGeometry(MeshRenderCommand* pCmd)
	{
		// find created item.
		auto key = GetGeometryKey(pCmd);
		auto find_it = staticGeomMap_.find(key);
		if (find_it != staticGeomMap_.end())
		{
			return find_it->second;
		}

		// create new geometry.
		auto geom = std::make_shared<BvhGeometry>();
		geom->Initialize(pCmd->GetParentMesh()->GetParentResource());
		staticGeomMap_[key] = geom;
		waitItems_.push_back(geom);
		return geom;
	}

	//----
	void BvhManager::BuildGeometry(CommandList* pCmdList)
	{
		// release prev frame compaction buffers.
		ReleaseCompactionBuffer();

		// build static geometry.
		u32 build_count = (maxBuildGeometryCount_ == 0) ? (u32)waitItems_.size() : std::min<u32>(maxBuildGeometryCount_, (u32)waitItems_.size());
		if (build_count > 0)
		{
			CreateCompactionBuffer(build_count);
			u32 info_index = 0;
			while (build_count > 0)
			{
				if (auto p = waitItems_[0].lock())
				{
					BuildStaticGeometry(pCmdList, p, info_index);
					waitItems_.erase(waitItems_.begin());
					info_index++;
				}
				build_count--;
			}
		}

		// compact static geometry.
		u32 compact_count = maxCompactGeometryCount_;
		for (auto&& p : staticGeomMap_)
		{
			auto geom = p.second;
			if (geom->isBuilt_ && geom->isCompactEnable && !geom->isCompacted)
			{
				if (geom->time_ > 0)
				{
					geom->time_--;
				}
				else
				{
					CompactStaticGeometry(pCmdList, geom);
					if (compact_count > 0)
					{
						compact_count--;
						if (compact_count == 0) break;
					}
				}
			}
		}
	}

	//----
	bool BvhManager::CreateCompactionBuffer(u32 maxCount)
	{
		pCompactionInfoBuffer_ = new Buffer();
		if (!pCompactionInfoBuffer_->Initialize(pDevice_,
			sizeof(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE_DESC) * maxCount,
			0,
			BufferUsage::ShaderResource,
			D3D12_RESOURCE_STATE_COMMON,
			false, true))
		{
			SafeDelete(pCompactionInfoBuffer_);
			return false;
		}

		// 情報コピー先バッファを生成する
		pCompactionReadback_ = std::make_shared<Buffer>();
		if (!pCompactionReadback_->Initialize(pDevice_,
			sizeof(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE_DESC) * maxCount,
			0,
			BufferUsage::ReadBack,
			D3D12_RESOURCE_STATE_COPY_DEST,
			false, false))
		{
			SafeDelete(pCompactionInfoBuffer_);
			return false;
		}

		return true;
	}

	//----
	void BvhManager::ReleaseCompactionBuffer()
	{
		if (pCompactionInfoBuffer_)
		{

			pDevice_->KillObject(pCompactionInfoBuffer_);
			pCompactionInfoBuffer_ = nullptr;
		}
		pCompactionReadback_.reset();
	}

	//----
	void BvhManager::CopyCompactionInfoOnGraphicsQueue(CommandList* pCmdList)
	{
		if (pCompactionInfoBuffer_)
		{
			pCmdList->TransitionBarrier(pCompactionInfoBuffer_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
			pCmdList->GetDxrCommandList()->CopyResource(pCompactionReadback_->GetResourceDep(), pCompactionInfoBuffer_->GetResourceDep());
		}
	}

	//----
	bool BvhManager::ReadyScratchBuffer(u64 size)
	{
		if (!pScratchBuffer_ || pScratchBuffer_->GetSize() < size)
		{
			if (pScratchBuffer_)
			{
				pDevice_->KillObject(pScratchBuffer_);
			}
			pScratchBuffer_ = new Buffer();
			if (!pScratchBuffer_->Initialize(pDevice_, size, 0, BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_COMMON, false, true))
			{
				return false;
			}
		}
		return true;
	}

	//----
	bool BvhManager::BuildStaticGeometry(CommandList* pCmdList, BvhGeometryPtr geom, u32 infoIndex)
	{
		// calc buffer size.
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input_desc{};
		input_desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		input_desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		input_desc.NumDescs = (UINT)geom->geomDescs_.size();
		input_desc.pGeometryDescs = geom->geomDescs_.data();
		input_desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{};
		pDevice_->GetDxrDeviceDep()->GetRaytracingAccelerationStructurePrebuildInfo(&input_desc, &prebuild_info);
		if (prebuild_info.ResultDataMaxSizeInBytes == 0)
		{
			return false;
		}

		// alloc bvh memory.
		geom->memInfo_ = allocator_->Alloc(prebuild_info.ResultDataMaxSizeInBytes);

		// ready scratch buffer.
		if (!ReadyScratchBuffer(prebuild_info.ScratchDataSizeInBytes))
		{
			return false;
		}

		// build bottom acceleration structure.
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc{};
		build_desc.DestAccelerationStructureData = geom->memInfo_.address_;
		build_desc.Inputs = input_desc;
		build_desc.ScratchAccelerationStructureData = pScratchBuffer_->GetResourceDep()->GetGPUVirtualAddress();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC info_desc{};
		info_desc.InfoType = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE;
		info_desc.DestBuffer = pCompactionInfoBuffer_->GetResourceDep()->GetGPUVirtualAddress()
			+ sizeof(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE_DESC) * infoIndex;

		pCmdList->GetLatestCommandList()->BuildRaytracingAccelerationStructure(
			&build_desc, 1, &info_desc);

		// barrier.
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.UAV.pResource = geom->memInfo_.pSuballocator_->pResource_;
		pCmdList->GetLatestCommandList()->ResourceBarrier(1, &barrier);

		geom->time_ = 3;		// time until getting compaction size.
		geom->isBuilt_ = true;
		geom->pCompactionReadback_ = pCompactionReadback_;
		geom->compactionIndex_ = infoIndex;

		return true;
	}

	//----
	bool BvhManager::CompactStaticGeometry(CommandList* pCmdList, BvhGeometryPtr geom)
	{
		// get compaction size.
		Buffer* pInfoBuffer = geom->pCompactionReadback_.get();
		auto pDesc = (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE_DESC*)pInfoBuffer->Map(nullptr);
		u64 size = pDesc[geom->compactionIndex_].CompactedSizeInBytes;
		pInfoBuffer->Unmap();
		geom->pCompactionReadback_.reset();

		// alloc new memory.
		BvhMemoryInfo new_mem = allocator_->Alloc(size);

		// copy compact bvh.
		pCmdList->GetLatestCommandList()->CopyRaytracingAccelerationStructure(
			new_mem.address_,
			geom->memInfo_.address_,
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT);

		// barrier.
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.UAV.pResource = new_mem.pSuballocator_->pResource_;
		pCmdList->GetLatestCommandList()->ResourceBarrier(1, &barrier);

		// free old memory.
		allocator_->Free(geom->memInfo_);

		geom->memInfo_ = new_mem;
		geom->isCompacted = true;

		return true;
	}

	//----
	BvhScene* BvhManager::BuildScene(CommandList* pCmdList, RenderCommandsList& cmds, u32 hitGroupCountPerMaterial, RenderCommandsTempList& outUsedCmds)
	{
		std::vector<BvhGeometryWeakPtr> geoms;
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> descs;
		geoms.reserve(cmds.size());
		descs.reserve(cmds.size());
		outUsedCmds.reserve(cmds.size());

		// find bvh geometries.
		u32 total_mat_count = 0;
		for (auto&& cmd : cmds)
		{
			// mesh.
			if (cmd->GetType() == RenderCommandType::Mesh)
			{
				auto mcmd = static_cast<MeshRenderCommand*>(cmd.get());
				auto geom = AddGeometry(mcmd).lock();
				if (geom && geom->IsValid())
				{
					auto&& mtx = mcmd->GetParentMesh()->GetMtxLocalToWorld();
					D3D12_RAYTRACING_INSTANCE_DESC desc{};

					desc.Transform[0][0] = mtx._11;
					desc.Transform[0][1] = mtx._21;
					desc.Transform[0][2] = mtx._31;
					desc.Transform[0][3] = mtx._41;
					desc.Transform[1][0] = mtx._12;
					desc.Transform[1][1] = mtx._22;
					desc.Transform[1][2] = mtx._32;
					desc.Transform[1][3] = mtx._42;
					desc.Transform[2][0] = mtx._13;
					desc.Transform[2][1] = mtx._23;
					desc.Transform[2][2] = mtx._33;
					desc.Transform[2][3] = mtx._43;
					desc.InstanceID = 0;
					desc.InstanceMask = 0xff;		// NOTE: if shadow on/off is needed, apply correct mask.
					desc.InstanceContributionToHitGroupIndex = total_mat_count * hitGroupCountPerMaterial;
					desc.Flags = 0;
					desc.AccelerationStructure = geom->memInfo_.address_;

					geoms.push_back(geom);
					descs.push_back(desc);
					outUsedCmds.push_back(cmd.get());
					total_mat_count += mcmd->GetParentMesh()->GetSubmeshCount();
				}
			}
		}

		if (geoms.empty())
		{
			return nullptr;
		}

		// calc buffer size.
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input_desc{};
		input_desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		input_desc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		input_desc.NumDescs = (UINT)descs.size();
		input_desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{};
		pDevice_->GetDxrDeviceDep()->GetRaytracingAccelerationStructurePrebuildInfo(&input_desc, &prebuild_info);
		if (prebuild_info.ResultDataMaxSizeInBytes == 0)
		{
			return nullptr;
		}

		// ready scratch buffer.
		if (!ReadyScratchBuffer(prebuild_info.ScratchDataSizeInBytes))
		{
			return nullptr;
		}

		// create instance buffer.
		Buffer* pInstBuffer = new Buffer();
		if (!pInstBuffer->Initialize(pDevice_, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * descs.size(), 0, sl12::BufferUsage::ShaderResource, true, false))
		{
			return nullptr;
		}

		auto p = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(pInstBuffer->Map(nullptr));
		memcpy(pInstBuffer->Map(nullptr), descs.data(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * descs.size());
		pInstBuffer->Unmap();

		// alloc bvh memory.
		auto mem_info = allocator_->Alloc(prebuild_info.ResultDataMaxSizeInBytes);

		// build top accelleration structure.
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc{};
		build_desc.DestAccelerationStructureData = mem_info.address_;
		build_desc.Inputs = input_desc;
		build_desc.Inputs.InstanceDescs = pInstBuffer->GetResourceDep()->GetGPUVirtualAddress();
		build_desc.ScratchAccelerationStructureData = pScratchBuffer_->GetResourceDep()->GetGPUVirtualAddress();

		pCmdList->GetLatestCommandList()->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);

		// barrier.
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.UAV.pResource = mem_info.pSuballocator_->pResource_;
		pCmdList->GetLatestCommandList()->ResourceBarrier(1, &barrier);

		// finalize.
		pDevice_->KillObject(pInstBuffer);
		BvhScene* ret = new BvhScene();
		ret->memInfo_ = mem_info;

		return ret;
	}

}	// namespace sl12


//	EOF
