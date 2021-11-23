#pragma once

#include <list>
#include <vector>
#include <memory>
#include <map>
#include "sl12/device.h"
#include "sl12/resource_mesh.h"
#include "sl12/buffer.h"
#include "sl12/render_command.h"
#include "sl12/scene_root.h"


namespace sl12
{
	//----------------
	class BvhMemorySuballocator
	{
		friend class BvhMemoryAllocator;
		friend class BvhManager;

	private:
		static const u32 kBlockSize = 256;	// bvh memory block size.

		struct Chunk
		{
			u32		head = 0;
			u32		count = 0;

			Chunk() {}
			Chunk(u32 h, u32 c)
				: head(h), count(c)
			{}
		};	//struct Chunk

	public:
		BvhMemorySuballocator(Device* pDev, size_t NeedSize);
		~BvhMemorySuballocator();

		bool Alloc(size_t size, D3D12_GPU_VIRTUAL_ADDRESS& address);
		void Free(D3D12_GPU_VIRTUAL_ADDRESS address, size_t size);

	private:
		ID3D12Resource*				pResource_ = nullptr;
		size_t						totalSize_ = 0;
		u32							totalBlockCount_ = 0;
		D3D12_GPU_VIRTUAL_ADDRESS	headAddress_;

		std::list<Chunk>			unusedChunks_;
	};	// class BvhMemorySuballocator

	//----------------
	class BvhMemoryInfo
	{
		friend class BvhMemoryAllocator;
		friend class BvhScene;
		friend class BvhManager;

	public:
		BvhMemoryInfo()
		{}
		BvhMemoryInfo(const BvhMemoryInfo& t)
			: address_(t.address_), size_(t.size_), pSuballocator_(t.pSuballocator_)
		{}

	private:
		BvhMemoryInfo(D3D12_GPU_VIRTUAL_ADDRESS a, size_t s, BvhMemorySuballocator* p)
			: address_(a), size_(s), pSuballocator_(p)
		{}

	private:
		D3D12_GPU_VIRTUAL_ADDRESS	address_ = 0;
		size_t						size_ = 0;
		BvhMemorySuballocator*		pSuballocator_ = nullptr;
	};	// class BvhAllocInfo

	//----------------
	class BvhMemoryAllocator
	{
	public:
		BvhMemoryAllocator(Device* pDev);
		~BvhMemoryAllocator();

		BvhMemoryInfo Alloc(size_t size);
		void Free(BvhMemoryInfo& info);

	private:
		Device*		pDevice_ = nullptr;
		std::vector<std::unique_ptr<BvhMemorySuballocator>>	suballocators_;
	};	// class BvhMemoryAllocator


	//----------------
	class BvhGeometry
	{
		friend class BvhManager;

	public:
		BvhGeometry();
		~BvhGeometry();

		bool Initialize(const ResourceItemMesh* pRes);
		bool IsValid() const;

	private:
		BvhMemoryInfo	memInfo_;
		s8				time_ = 0;
		bool			isBuilt_ = false;
		bool			isCompacted = false;
		bool			isCompactEnable = false;

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>	geomDescs_;

		std::shared_ptr<Buffer>	pCompactionReadback_;
		u32						compactionIndex_ = 0;
	};	// class BvhGeometry
	using BvhGeometryPtr = std::shared_ptr<BvhGeometry>;
	using BvhGeometryWeakPtr = std::weak_ptr<BvhGeometry>;

	//----------------
	class BvhScene
	{
		friend class BvhManager;

	public:
		BvhScene();
		~BvhScene();

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const
		{
			return memInfo_.address_;
		}

	private:
		BvhMemoryInfo	memInfo_;
	};	// class BvhScene

	//----------------
	class BvhManager
	{
	private:
		using GeometryKey = const ResourceItemMesh*;

	public:
		BvhManager(Device* pDev);
		~BvhManager();

		BvhGeometryWeakPtr AddGeometry(MeshRenderCommand* pCmd);

		void BuildGeometry(CommandList* pCmdList);
		BvhScene* BuildScene(
			CommandList* pCmdList,					// rhi command list.
			RenderCommandsList& cmds,				// render commands from scene root.
			u32 hitGroupCountPerMaterial,			// hit groups count per material.
			RenderCommandsTempList& outUsedCmds		// commands used to build bvh.
		);

		void CopyCompactionInfoOnGraphicsQueue(CommandList* pCmdList);

	private:
		GeometryKey GetGeometryKey(MeshRenderCommand* pCmd);

		bool CreateCompactionBuffer(u32 maxCount);
		void ReleaseCompactionBuffer();

		bool ReadyScratchBuffer(u64 size);
		bool BuildStaticGeometry(CommandList* pCmdList, BvhGeometryPtr geom, u32 infoIndex);
		bool CompactStaticGeometry(CommandList* pCmdList, BvhGeometryPtr geom);

	private:
		Device*		pDevice_ = nullptr;
		u32			maxBuildGeometryCount_ = 0;
		u32			maxCompactGeometryCount_ = 0;

		std::unique_ptr<BvhMemoryAllocator>		allocator_;
		std::map<GeometryKey, BvhGeometryPtr>	staticGeomMap_;
		std::vector<BvhGeometryWeakPtr>			waitItems_;

		Buffer*		pScratchBuffer_ = nullptr;
		Buffer*		pCompactionInfoBuffer_ = nullptr;
		std::shared_ptr<Buffer>	pCompactionReadback_;
	};	// class BvhManager

}	// namespace sl12


//	EOF
