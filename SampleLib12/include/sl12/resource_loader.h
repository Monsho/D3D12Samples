#pragma once

#include "sl12/types.h"

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>


namespace sl12
{
	class Device;
	class ResourceItemBase;
	class MeshManager;

	constexpr u32 TYPE_FOURCC(const char* s)
	{
		return ((u32(s[0]) << 24) | (u32(s[1]) << 16) | (u32(s[2]) << 8) | (u32(s[3]) << 0));
	}

	class ResourceItemBase
	{
		friend class ResourceLoader;

	public:
		static const u32 kType = TYPE_FOURCC("BASE");

		ResourceItemBase(u32 tid)
			: typeID_(tid)
		{}
		virtual ~ResourceItemBase()
		{}

		const std::string& GetFilePath() const
		{
			return filePath_;
		}
		const u32 GetTypeID() const
		{
			return typeID_;
		}

	protected:
		ResourceLoader* pParentLoader_ = nullptr;
		std::string		filePath_;
		u32				typeID_;
	};	// class ResourceItemBase

	class ResourceHandle
	{
		friend class ResourceLoader;

	public:
		ResourceHandle()
		{}
		ResourceHandle(const ResourceHandle& rhs)
			: pParentLoader_(rhs.pParentLoader_), id_(rhs.id_)
		{}

		bool IsValid() const;

		u64 GetID() const
		{
			return id_;
		}

		const ResourceItemBase* GetItemBase() const;

		template <typename T>
		const T* GetItem() const
		{
			auto ret = GetItemBase();
			if (!ret || ret->GetTypeID() != T::kType)
			{
				return nullptr;
			}
			return static_cast<const T*>(ret);
		}

	private:
		ResourceHandle(ResourceLoader* loader, u64 id)
			: pParentLoader_(loader), id_(id)
		{}

		ResourceLoader*	pParentLoader_ = nullptr;
		u64				id_ = 0;
	};	// class ResourceHandle

	class ResourceLoader
	{
		friend class ResourceHandle;

	public:
		typedef ResourceItemBase*	(*LoadFunc)(ResourceLoader*, const std::string&);

		ResourceLoader()
		{}
		~ResourceLoader();

		bool Initialize(Device* pDevice);
		void Destroy();

		ResourceHandle LoadRequest(const std::string& filepath, LoadFunc func);

		template <typename T>
		ResourceHandle LoadRequest(const std::string& filepath)
		{
			return LoadRequest(filepath, T::LoadFunction);
		}

		Device* GetDevice()
		{
			return pDevice_;
		}

		void SetMeshManager(MeshManager* p)
		{
			pMeshManager_ = p;
		}
		MeshManager* GetMeshManager()
		{
			return pMeshManager_;
		}

		bool IsLoading() const
		{
			return !requestList_.empty() || isLoading_;
		}

	private:
		const ResourceItemBase* GetItemBaseFromID(u64 id) const;

	private:
		struct RequestItem
		{
			u64				id;
			std::string		filePath;
			LoadFunc		funcLoad;
		};	// struct RequestItem

		Device*				pDevice_ = nullptr;
		MeshManager*		pMeshManager_ = nullptr;
		std::atomic<u64>	handleID_ = 0;

		std::map<u64, std::unique_ptr<ResourceItemBase>>	resourceMap_;

		std::mutex				requestMutex_;
		std::mutex				listMutex_;
		std::condition_variable	requestCV_;
		std::thread				loadingThread_;
		std::list<RequestItem>	requestList_;
		bool					isAlive_ = false;
		bool					isLoading_ = false;
	};	// class ResourceLoader

}	// namespace sl12


//	EOF
