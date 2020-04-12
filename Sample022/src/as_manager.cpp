#include "as_manager.h"

#include "mesh_instance.h"


namespace sl12
{
	//----
	bool BlasItem::Build(Device* pDevice, CommandList* pCmdList)
	{
		if (isInitialized_)
			return true;

		pBlas_ = new BottomAccelerationStructure();

		ResourceItemMesh* pLocalMesh = const_cast<ResourceItemMesh*>(pMeshItem_);
		auto&& submeshes = pLocalMesh->GetSubmeshes();
		std::vector<GeometryStructureDesc> geoDescs(submeshes.size());
		for (int i = 0; i < submeshes.size(); i++)
		{
			auto&& submesh = submeshes[i];
			auto&& material = pLocalMesh->GetMaterials()[submesh.materialIndex];

			auto flags = material.isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			geoDescs[i].InitializeAsTriangle(
				flags,
				&pLocalMesh->GetPositionVB(),
				&pLocalMesh->GetIndexBuffer(),
				nullptr,
				pLocalMesh->GetPositionVB().GetStride(),
				submesh.vertexCount,
				submesh.positionVBV.GetBufferOffset(),
				DXGI_FORMAT_R32G32B32_FLOAT,
				submesh.indexCount,
				submesh.indexBV.GetBufferOffset(),
				DXGI_FORMAT_R32_UINT);
		}

		StructureInputDesc bottomInput{};
		if (!bottomInput.InitializeAsBottom(pDevice, geoDescs.data(), (u32)submeshes.size(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			delete pBlas_;
			return false;
		}

		if (!pBlas_->CreateBuffer(pDevice, bottomInput.prebuildInfo.ResultDataMaxSizeInBytes, bottomInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			delete pBlas_;
			return false;
		}

		// コマンド発行
		if (!pBlas_->Build(pCmdList, bottomInput))
		{
			delete pBlas_;
			return false;
		}

		isInitialized_ = true;

		return true;
	}

	//----
	void BlasItem::Release(Device* pDevice)
	{
		pDevice->KillObject(pBlas_);
		delete this;
	}


	//----
	ASManager::~ASManager()
	{
		if (pParentDevice_)
		{
			for (auto&& item : blasMap_)
				item.second->Release(pParentDevice_);
			blasMap_.clear();

			pParentDevice_->KillObject(pTlas_);
		}
	}

	//----
	void ASManager::EntryMeshItem(ResourceHandle hMesh)
	{
		auto find_it = blasMap_.find(hMesh.GetID());
		if (find_it != blasMap_.end())
			return;

		auto pMeshItem = hMesh.GetItem<ResourceItemMesh>();
		if (pMeshItem)
		{
			auto as_item = new BlasItem(pMeshItem);
			blasMap_[hMesh.GetID()] = as_item;
		}
	}

	//----
	bool ASManager::Build(CommandList* pCmdList, TlasInstance* pInstances, u32 instanceCount, u32 recordCountPerMaterial)
	{
		// build bottom as.
		struct BLAS
		{
			int			tableOffset;
			BlasItem*	pBLAS;
		};
		std::map<u64, BLAS> blass_;
		int total_submesh_count = 0;
		for (auto item : blasMap_)
		{
			if (!item.second->Build(pParentDevice_, pCmdList))
				return false;

			BLAS blas = { total_submesh_count, item.second };
			blass_[item.first] = blas;
			total_submesh_count += item.second->GetSubmeshCount();
		}
		totalMaterialCount_ = total_submesh_count;

		// set top as instances.
		std::vector<TopInstanceDesc> instance_descs(instanceCount);
		for (u32 idx = 0; idx < instanceCount; idx++)
		{
			auto&& inst = pInstances[idx];
			auto&& blas = blass_[inst.pInstance->GetResMesh().GetID()];

			instance_descs[idx].Initialize(
				inst.pInstance->GetMtxTransform(),
				idx,
				inst.mask,
				blas.tableOffset * recordCountPerMaterial,
				inst.flags,
				blas.pBLAS->GetBlas());
		}

		// build top as.
		{
			if (pTlas_)
			{
				pParentDevice_->KillObject(pTlas_);
				pTlas_ = nullptr;
			}
			pTlas_ = new TopAccelerationStructure();

			sl12::StructureInputDesc topInput{};
			if (!topInput.InitializeAsTop(pParentDevice_, instanceCount, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
			{
				return false;
			}

			if (!pTlas_->CreateBuffer(pParentDevice_, topInput.prebuildInfo.ResultDataMaxSizeInBytes, topInput.prebuildInfo.ScratchDataSizeInBytes))
			{
				return false;
			}
			if (!pTlas_->CreateInstanceBuffer(pParentDevice_, instance_descs.data(), instanceCount))
			{
				return false;
			}

			// コマンド発行
			if (!pTlas_->Build(pCmdList, topInput))
			{
				return false;
			}
		}

		return true;
	}

	bool ASManager::CreateMaterialTable(std::function<bool(BlasItem*)> setMaterialFunc)
	{
		for (auto item : blasMap_)
		{
			if (!setMaterialFunc(item.second))
			{
				return false;
			}
		}
		return true;
	}

	bool ASManager::CreateHitGroupTable(
		u32 recordSize,
		u32 recordCountPerMaterial,
		std::function<bool(u8*, u32)> setFunc,
		Buffer& outTableBuffer)
	{
		// initialize buffer.
		u64 buff_size = recordSize * recordCountPerMaterial * totalMaterialCount_;
		if (!outTableBuffer.Initialize(pParentDevice_, buff_size, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
		{
			return false;
		}

		// map.
		auto p = (u8*)outTableBuffer.Map(nullptr);

		// set records.
		for (u32 i = 0; i < totalMaterialCount_; i++)
		{
			if (!setFunc(p, i))
			{
				outTableBuffer.Unmap();
				return false;
			}
			p += recordSize * recordCountPerMaterial;
		}

		// unmap.
		outTableBuffer.Unmap();

		return true;
	}

}	// namespace sl12


//	EOF
