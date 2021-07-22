#pragma once

#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/texture_view.h"
#include "sl12/resource_mesh.h"


class SceneSubmesh
{
	friend class SceneMesh;

	struct MeshletCB
	{
		sl12::u32	meshletCount;
		sl12::u32	indirectArg1stIndexOffset;
		sl12::u32	indirectArg2ndIndexOffset;
		sl12::u32	indirectCount1stByteOffset;
		sl12::u32	indirectCount2ndByteOffset;
		sl12::u32	falseNegativeIndexOffset;
		sl12::u32	falseNegativeCountByteOffset;
	};	// struct MeshletCB

public:
	SceneSubmesh(sl12::Device* pDevice, const sl12::ResourceItemMesh::Submesh& submesh);
	~SceneSubmesh();

	const MeshletCB& GetMeshletData() const
	{
		return cbData_;
	}

	sl12::BufferView* GetMeshletBoundsBV()
	{
		return pMeshletBoundsBV_;
	}
	sl12::BufferView* GetMeshletDrawInfoBV()
	{
		return pMeshletDrawInfoBV_;
	}
	sl12::ConstantBufferView* GetMeshletCBV()
	{
		return pMeshletCBV_;
	}

private:
	sl12::Device*				pParentDevice_ = nullptr;

	sl12::Buffer*				pMeshletBoundsB_ = nullptr;
	sl12::Buffer*				pMeshletDrawInfoB_ = nullptr;
	sl12::BufferView*			pMeshletBoundsBV_ = nullptr;
	sl12::BufferView*			pMeshletDrawInfoBV_ = nullptr;

	MeshletCB					cbData_;
	sl12::Buffer*				pMeshletCB_ = nullptr;
	sl12::ConstantBufferView*	pMeshletCBV_ = nullptr;
};	// class SceneSubmesh

class SceneMesh
{
public:
	SceneMesh(sl12::Device* pDevice, const sl12::ResourceItemMesh* pSrcMesh);
	~SceneMesh();

	sl12::u32 GetSubmeshCount() const
	{
		return (sl12::u32)sceneSubmeshes_.size();
	}
	SceneSubmesh* GetSubmesh(sl12::u32 index)
	{
		return sceneSubmeshes_[index].get();
	}

	sl12::Buffer* GetIndirectArgBuffer()
	{
		return pIndirectArgBuffer_;
	}
	sl12::Buffer* GetIndirectCountBuffer()
	{
		return pIndirectCountBuffer_;
	}
	sl12::Buffer* GetFalseNegativeBuffer()
	{
		return pFalseNegativeBuffer_;
	}
	sl12::Buffer* GetFalseNegativeCountBuffer()
	{
		return pFalseNegativeCountBuffer_;
	}
	sl12::UnorderedAccessView* GetIndirectArgUAV()
	{
		return pIndirectArgUAV_;
	}
	sl12::UnorderedAccessView* GetIndirectCountUAV()
	{
		return pIndirectCountUAV_;
	}
	sl12::UnorderedAccessView* GetFalseNegativeUAV()
	{
		return pFalseNegativeUAV_;
	}
	sl12::UnorderedAccessView* GetFalseNegativeCountUAV()
	{
		return pFalseNegativeCountUAV_;
	}

	const DirectX::XMFLOAT4X4& GetMtxLocalToWorld() const
	{
		return mtxLocalToWorld_;
	}
	void SetMtxLocalToWorld(const DirectX::XMFLOAT4X4& m)
	{
		mtxLocalToWorld_ = m;
	}

private:
	sl12::Device*				pParentDevice_ = nullptr;

	std::vector<std::unique_ptr<SceneSubmesh>>	sceneSubmeshes_;

	sl12::Buffer*				pIndirectArgBuffer_ = nullptr;
	sl12::Buffer*				pIndirectCountBuffer_ = nullptr;
	sl12::Buffer*				pFalseNegativeBuffer_ = nullptr;
	sl12::Buffer*				pFalseNegativeCountBuffer_ = nullptr;
	sl12::UnorderedAccessView*	pIndirectArgUAV_ = nullptr;
	sl12::UnorderedAccessView*	pIndirectCountUAV_ = nullptr;
	sl12::UnorderedAccessView*	pFalseNegativeUAV_ = nullptr;
	sl12::UnorderedAccessView*	pFalseNegativeCountUAV_ = nullptr;

	DirectX::XMFLOAT4X4	mtxLocalToWorld_;
};	// class SceneMesh


//	EOF
