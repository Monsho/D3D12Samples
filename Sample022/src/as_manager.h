#pragma once

#include "sl12/device.h"
#include "sl12/command_list.h"
#include "sl12/acceleration_structure.h"
#include "sl12/resource_mesh.h"

#include <map>


namespace sl12
{
	class BlasItem
	{
	public:
		BlasItem(const ResourceItemMesh* pMesh)
			: pMeshItem_(pMesh)
		{}

		const ResourceItemMesh* GetMeshItem() const
		{
			return pMeshItem_;
		}
		int GetSubmeshCount() const
		{
			return (int)pMeshItem_->GetSubmeshes().size();
		}
		BottomAccelerationStructure* GetBlas()
		{
			return pBlas_;
		}

		bool Build(Device* pDevice, CommandList* pCmdList);
		void Release(Device* pDevice);

	private:
		~BlasItem()
		{}

		const ResourceItemMesh*			pMeshItem_ = nullptr;
		BottomAccelerationStructure*	pBlas_ = nullptr;
		bool							isInitialized_ = false;
	};	// class BlasItem

	struct TlasInstance
	{
		class MeshInstance*		pInstance = nullptr;
		u32						mask = 0xff;
		u8						flags = 0;
	};	// struct TlasInstance

	class ASManager
	{
	public:
		ASManager(Device* pDev)
			: pParentDevice_(pDev)
		{}
		~ASManager();

		void EntryMeshItem(ResourceHandle hMesh);

		bool Build(CommandList* pCmdList, TlasInstance* ppInstances, u32 instanceCount, u32 recordCountPerMaterial);

		bool CreateHitGroupTable(
			u32 recordSize,
			u32 recordCountPerMaterial,
			std::function<u8*(BlasItem*, u8*)> setFunc,
			Buffer& outTableBuffer);

	private:
		Device*						pParentDevice_ = nullptr;
		std::map<u64, BlasItem*>	blasMap_;
		TopAccelerationStructure*	pTlas_ = nullptr;
	};	// class ASManager

}	// namespace sl12


//	EOF
