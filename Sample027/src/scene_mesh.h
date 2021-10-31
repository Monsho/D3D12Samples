﻿#pragma once

#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/texture_view.h"
#include "sl12/resource_mesh.h"
#include "render_command.h"
#include "scene_root.h"


namespace sl12
{
	class SceneSubmesh
	{
		friend class SceneMesh;

		struct MeshletCB
		{
			u32	meshletCount;
			u32	indirectArg1stIndexOffset;
			u32	indirectArg2ndIndexOffset;
			u32	indirectCount1stByteOffset;
			u32	indirectCount2ndByteOffset;
			u32	falseNegativeIndexOffset;
			u32	falseNegativeCountByteOffset;
		};	// struct MeshletCB

	public:
		SceneSubmesh(Device* pDevice, const ResourceItemMesh::Submesh& submesh);
		~SceneSubmesh();

		const MeshletCB& GetMeshletData() const
		{
			return cbData_;
		}

		BufferView* GetMeshletBoundsBV()
		{
			return pMeshletBoundsBV_;
		}
		BufferView* GetMeshletDrawInfoBV()
		{
			return pMeshletDrawInfoBV_;
		}
		ConstantBufferView* GetMeshletCBV()
		{
			return pMeshletCBV_;
		}

	private:
		Device*		pParentDevice_ = nullptr;

		Buffer*		pMeshletBoundsB_ = nullptr;
		Buffer*		pMeshletDrawInfoB_ = nullptr;
		BufferView* pMeshletBoundsBV_ = nullptr;
		BufferView* pMeshletDrawInfoBV_ = nullptr;

		MeshletCB	cbData_;
		Buffer*		pMeshletCB_ = nullptr;
		ConstantBufferView* pMeshletCBV_ = nullptr;
	};	// class SceneSubmesh

	class SceneMesh
		: public SceneNode
	{
	public:
		SceneMesh(Device* pDevice, const ResourceItemMesh* pSrcMesh);
		~SceneMesh();

		void CreateRenderCommand(ConstantBufferCache* pCBCache, RenderCommandsList& outRenderCmds) override;

		const ResourceItemMesh* GetParentResource() const
		{
			return pParentResource_;
		}

		u32 GetSubmeshCount() const
		{
			return (u32)sceneSubmeshes_.size();
		}
		SceneSubmesh* GetSubmesh(u32 index)
		{
			return sceneSubmeshes_[index].get();
		}

		Buffer* GetIndirectArgBuffer()
		{
			return pIndirectArgBuffer_;
		}
		Buffer* GetIndirectCountBuffer()
		{
			return pIndirectCountBuffer_;
		}
		Buffer* GetFalseNegativeBuffer()
		{
			return pFalseNegativeBuffer_;
		}
		Buffer* GetFalseNegativeCountBuffer()
		{
			return pFalseNegativeCountBuffer_;
		}
		UnorderedAccessView* GetIndirectArgUAV()
		{
			return pIndirectArgUAV_;
		}
		UnorderedAccessView* GetIndirectCountUAV()
		{
			return pIndirectCountUAV_;
		}
		UnorderedAccessView* GetFalseNegativeUAV()
		{
			return pFalseNegativeUAV_;
		}
		UnorderedAccessView* GetFalseNegativeCountUAV()
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
		const DirectX::XMFLOAT4X4& GetMtxPrevLocalToWorld() const
		{
			return mtxPrevLocalToWorld_;
		}

	private:
		Device*	pParentDevice_ = nullptr;
		const ResourceItemMesh*	pParentResource_ = nullptr;

		std::vector<std::unique_ptr<SceneSubmesh>>	sceneSubmeshes_;

		Buffer*	pIndirectArgBuffer_ = nullptr;
		Buffer*	pIndirectCountBuffer_ = nullptr;
		Buffer*	pFalseNegativeBuffer_ = nullptr;
		Buffer*	pFalseNegativeCountBuffer_ = nullptr;
		UnorderedAccessView*	pIndirectArgUAV_ = nullptr;
		UnorderedAccessView*	pIndirectCountUAV_ = nullptr;
		UnorderedAccessView*	pFalseNegativeUAV_ = nullptr;
		UnorderedAccessView*	pFalseNegativeCountUAV_ = nullptr;

		DirectX::XMFLOAT4X4	mtxLocalToWorld_;
		DirectX::XMFLOAT4X4	mtxPrevLocalToWorld_;
	};	// class SceneMesh

}	// namespace sl12


//	EOF
