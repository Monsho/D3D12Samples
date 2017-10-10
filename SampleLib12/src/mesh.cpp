#include "sl12/mesh.h"


namespace sl12
{
	//---------------------------------------
	// 初期化する
	//---------------------------------------
	bool MeshShapeInstance::Initialize(sl12::Device* pDev, sl12::CommandList* pCmdList, const MeshShape* shape, const void* p_vertex_head)
	{
		assert(shape != nullptr);
		assert(p_vertex_head != nullptr);

		pSrcShape_ = shape;

		auto vbInitFunc = [&](VertexBuffer& vb, size_t stride, size_t offset)
		{
			if (!vb.buffer_.Initialize(pDev, stride * shape->numVertices, stride, sl12::BufferUsage::VertexBuffer, false, false))
			{
				return false;
			}
			if (!vb.view_.Initialize(pDev, &vb.buffer_))
			{
				return false;
			}
		
			vb.buffer_.UpdateBuffer(pDev, pCmdList, reinterpret_cast<const u8*>(p_vertex_head) + offset, stride * shape->numVertices);
			
			return true;
		};

		// 座標
		if (!vbInitFunc(vbPosition_, sizeof(float) * 3, shape->positionOffset))
		{
			return false;
		}

		// 法線
		if (!vbInitFunc(vbNormal_, sizeof(float) * 3, shape->normalOffset))
		{
			return false;
		}

		// テクスチャ座標
		if (!vbInitFunc(vbTexcoord_, sizeof(float) * 2, shape->texcoordOffset))
		{
			return false;
		}

		return true;
	}

	//---------------------------------------
	// 破棄する
	//---------------------------------------
	void MeshShapeInstance::Destroy()
	{
		vbPosition_.~VertexBuffer();
		vbNormal_.~VertexBuffer();
		vbTexcoord_.~VertexBuffer();
	}


	//---------------------------------------
	// 初期化する
	//---------------------------------------
	bool MeshSubmeshInstance::Initialize(sl12::Device* pDev, sl12::CommandList* pCmdList, const MeshSubmesh* submesh, const void* p_vertex_head)
	{
		assert(submesh != nullptr);
		assert(p_vertex_head != nullptr);

		pSrcSubmesh_ = submesh;

		if (!indexBuffer_.buffer_.Initialize(pDev, sizeof(u32) * submesh->numSubmeshIndices, sizeof(u32), BufferUsage::IndexBuffer, false, false))
		{
			return false;
		}
		if (!indexBuffer_.view_.Initialize(pDev, &indexBuffer_.buffer_))
		{
			return false;
		}

		indexBuffer_.buffer_.UpdateBuffer(pDev, pCmdList, reinterpret_cast<const u8*>(p_vertex_head) + submesh->indexBufferOffset, sizeof(u32) * submesh->numSubmeshIndices);

		return true;
	}

	//---------------------------------------
	// 破棄する
	//---------------------------------------
	void MeshSubmeshInstance::Destroy()
	{
		indexBuffer_.~IndexBuffer();
	}


	//---------------------------------------
	// 初期化する
	//---------------------------------------
	bool MeshInstance::Initialize(sl12::Device* pDev, sl12::CommandList* pCmdList, const void* pBin)
	{
		assert(pDev != nullptr);
		assert(pCmdList != nullptr);
		assert(pBin != nullptr);

		// ヘッダを確認
		pHead_ = reinterpret_cast<const MeshHead*>(pBin);
		if (pHead_->fourCC[0] != 'M' || pHead_->fourCC[1] != 'E' || pHead_->fourCC[2] != 'S' || pHead_->fourCC[3] != 'H')
		{
			return false;
		}

		const MeshShape* pSrcShapes = reinterpret_cast<const MeshShape*>(pHead_ + 1);
		const MeshMaterial* pSrcMaterials = reinterpret_cast<const MeshMaterial*>(pSrcShapes + pHead_->numShapes);
		const MeshSubmesh* pSrcSubmeshes = reinterpret_cast<const MeshSubmesh*>(pSrcMaterials + pHead_->numMaterials);
		const void* pVertexHead = pSrcSubmeshes + pHead_->numSubmeshes;

		pMaterials_ = pSrcMaterials;
		pShapes_ = new MeshShapeInstance[pHead_->numShapes];
		pSubmeshes_ = new MeshSubmeshInstance[pHead_->numSubmeshes];
		assert(pShapes_ != nullptr);
		assert(pSubmeshes_ != nullptr);

		// シェイプの初期化
		for (s32 i = 0; i < pHead_->numShapes; ++i)
		{
			if (!pShapes_[i].Initialize(pDev, pCmdList, &pSrcShapes[i], pVertexHead))
			{
				return false;
			}
		}

		// サブメッシュの初期化
		for (s32 i = 0; i < pHead_->numSubmeshes; ++i)
		{
			if (!pSubmeshes_[i].Initialize(pDev, pCmdList, &pSrcSubmeshes[i], pVertexHead))
			{
				return false;
			}
		}

		return true;
	}

	//---------------------------------------
	// 破棄する
	//---------------------------------------
	void MeshInstance::Destroy()
	{
		sl12::SafeDeleteArray(pShapes_);
		sl12::SafeDeleteArray(pSubmeshes_);
		pHead_ = nullptr;
		pMaterials_ = nullptr;
	}

}	// namespace sl12


//	EOF
