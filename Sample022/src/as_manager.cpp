#include "as_manager.h"


//----
bool BottomASItem::Build(sl12::Device* pDevice, sl12::CommandList* pCmdList)
{
	if (isInitialized_)
		return true;

	pBottomAS_ = new sl12::BottomAccelerationStructure();

	sl12::ResourceItemMesh* pLocalMesh = const_cast<sl12::ResourceItemMesh*>(pMeshItem_);
	auto&& submeshes = pLocalMesh->GetSubmeshes();
	std::vector<sl12::GeometryStructureDesc> geoDescs(submeshes.size());
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

	sl12::StructureInputDesc bottomInput{};
	if (!bottomInput.InitializeAsBottom(pDevice, geoDescs.data(), (sl12::u32)submeshes.size(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
	{
		delete pBottomAS_;
		return false;
	}

	if (!pBottomAS_->CreateBuffer(pDevice, bottomInput.prebuildInfo.ResultDataMaxSizeInBytes, bottomInput.prebuildInfo.ScratchDataSizeInBytes))
	{
		delete pBottomAS_;
		return false;
	}

	// コマンド発行
	if (!pBottomAS_->Build(pCmdList, bottomInput))
	{
		delete pBottomAS_;
		return false;
	}

	isInitialized_ = true;

	return true;
}

//----
void BottomASItem::Release(sl12::Device* pDevice)
{
	pDevice->KillObject(pBottomAS_);
	delete this;
}


//----
ASManager::~ASManager()
{
	for (auto&& item : bottomASMap_)
		item.second->Release(pParentDevice_);
	bottomASMap_.clear();
}

//----
void ASManager::EntryMeshItem(sl12::ResourceHandle hMesh)
{
	auto find_it = bottomASMap_.find(hMesh.GetID());
	if (find_it != bottomASMap_.end())
		return;

	auto pMeshItem = hMesh.GetItem<sl12::ResourceItemMesh>();
	if (pMeshItem)
	{
		auto as_item = new BottomASItem(pMeshItem);
		bottomASMap_[hMesh.GetID()] = as_item;
	}
}

//----
bool ASManager::Build(sl12::CommandList* pCmdList)
{
	// build bottom as.
	struct BLAS
	{
		int				tableOffset;
		BottomASItem*	pBLAS;
	};
	std::map<sl12::u64, BLAS> blass_;
	int total_submesh_count = 0;
	for (auto item : bottomASMap_)
	{
		if (!item.second->Build(pParentDevice_, pCmdList))
			return false;

		BLAS blas = { total_submesh_count, item.second };
		blass_[item.first] = blas;
		total_submesh_count += item.second->GetSubmeshCount();
	}

	// build top as.

	return true;
}

//	EOF
