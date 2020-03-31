#pragma once

#include "sl12/device.h"
#include "sl12/command_list.h"
#include "sl12/acceleration_structure.h"
#include "sl12/resource_mesh.h"

#include <map>


class BottomASItem
{
public:
	BottomASItem(const sl12::ResourceItemMesh* pMesh)
		: pMeshItem_(pMesh)
	{}

	const sl12::ResourceItemMesh* GetMeshItem() const
	{
		return pMeshItem_;
	}
	int GetSubmeshCount() const
	{
		return (int)pMeshItem_->GetSubmeshes().size();
	}
	sl12::BottomAccelerationStructure* GetBottomAS()
	{
		return pBottomAS_;
	}

	bool Build(sl12::Device* pDevice, sl12::CommandList* pCmdList);
	void Release(sl12::Device* pDevice);

private:
	~BottomASItem()
	{}

	const sl12::ResourceItemMesh*		pMeshItem_ = nullptr;
	sl12::BottomAccelerationStructure*	pBottomAS_ = nullptr;
	bool								isInitialized_ = false;
};	// class BottomASItem

class ASManager
{
public:
	ASManager(sl12::Device* pDev)
		: pParentDevice_(pDev)
	{}
	~ASManager();

	void EntryMeshItem(sl12::ResourceHandle hMesh);

	bool Build(sl12::CommandList* pCmdList);

private:
	sl12::Device*						pParentDevice_ = nullptr;
	std::map<sl12::u64, BottomASItem*>	bottomASMap_;
};	// class ASManager

//	EOF
