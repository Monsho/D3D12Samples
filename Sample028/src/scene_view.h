#pragma once

#include "sl12/scene_root.h"


namespace sl12
{
	class Device;
	class CommandList;
	class Buffer;

	//----
	class SceneView
	{
	public:
		struct BatchInfo
		{
			u32		instanceIndex;
			u32		submeshIndex;
			u32		indirectArgByteOffset;
			u32		drawCountByteOffset;
		};	// struct BatchInfo

	public:
		SceneView(Device* pDev);
		~SceneView();

		void BeginNewFrame(CommandList* pCmdList, SceneRoot* pSceneRoot, RenderCommandsList& RenderCmds);

		// getter
		std::vector<MeshRenderCommand*>& GetDrawMeshCmds()
		{
			return drawMeshCmds_;
		}
		std::vector<BatchInfo>& GetBatchInfos()
		{
			return batchInfos_;
		}

		u32 GetCullUnitCount() const
		{
			return cullUnitCount_;
		}
		u32 GetBatchCount() const
		{
			return batchCount_;
		}

		Buffer* GetIndirectArg()
		{
			return pIndirectArg_;
		}
		Buffer* GetDrawCount()
		{
			return pDrawCount_;
		}
		Buffer* GetFalseNegativeIndex()
		{
			return pFalseNegativeIndex_;
		}
		Buffer* GetFalseNegativeIndirect()
		{
			return pFalseNegativeIndirect_;
		}

		ConstantBufferView* GetConstantCBV()
		{
			return pConstantCBV_;
		}
		BufferView* GetCullUnitBV()
		{
			return pCullUnitBV_;
		}
		UnorderedAccessView* GetIndirectArgUAV()
		{
			return pIndirectArgUAV_;
		}
		UnorderedAccessView* GetDrawCountUAV()
		{
			return pDrawCountUAV_;
		}
		BufferView* GetFalseNegativeIndexBV()
		{
			return pFalseNegativeIndexBV_;
		}
		UnorderedAccessView* GetFalseNegativeIndexUAV()
		{
			return pFalseNegativeIndexUAV_;
		}
		UnorderedAccessView* GetFalseNegativeIndirectUAV()
		{
			return pFalseNegativeIndirectUAV_;
		}
		BufferView* GetInstanceDataBV()
		{
			return pInstanceDataBV_;
		}
		BufferView* GetBatchDataBV()
		{
			return pBatchDataBV_;
		}
		BufferView* GetMeshletBoundDataBV()
		{
			return pMeshletBoundDataBV_;
		}
		BufferView* GetMeshletDrawDataBV()
		{
			return pMeshletDrawDataBV_;
		}

	private:
		void ReleaseBuffers();

	private:
		Device*		pParentDevice_ = nullptr;

		std::vector<MeshRenderCommand*>	drawMeshCmds_;
		std::vector<BatchInfo>			batchInfos_;

		u32			cullUnitCount_ = 0;
		u32			batchCount_ = 0;

		Buffer*		pConstant_ = nullptr;
		Buffer*		pCullUnit_ = nullptr;
		Buffer*		pIndirectArg_ = nullptr;
		Buffer*		pDrawCount_ = nullptr;
		Buffer*		pFalseNegativeIndex_ = nullptr;
		Buffer*		pFalseNegativeIndirect_ = nullptr;
		Buffer*		pInstanceData_ = nullptr;
		Buffer*		pBatchData_ = nullptr;
		Buffer*		pMeshletBoundData_ = nullptr;
		Buffer*		pMeshletDrawData_ = nullptr;

		ConstantBufferView*		pConstantCBV_ = nullptr;
		BufferView*				pCullUnitBV_ = nullptr;
		UnorderedAccessView*	pIndirectArgUAV_ = nullptr;
		UnorderedAccessView*	pDrawCountUAV_ = nullptr;
		BufferView*				pFalseNegativeIndexBV_ = nullptr;
		UnorderedAccessView*	pFalseNegativeIndexUAV_ = nullptr;
		UnorderedAccessView*	pFalseNegativeIndirectUAV_ = nullptr;
		BufferView*				pInstanceDataBV_ = nullptr;
		BufferView*				pBatchDataBV_ = nullptr;
		BufferView*				pMeshletBoundDataBV_ = nullptr;
		BufferView*				pMeshletDrawDataBV_ = nullptr;
	};	// class SceneView

}	// namespace sl12

//	EOF
