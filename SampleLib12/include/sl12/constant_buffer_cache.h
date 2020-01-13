#pragma once

#include <sl12/device.h>
#include <sl12/buffer.h>
#include <sl12/buffer_view.h>
#include <list>
#include <map>


namespace sl12
{
	class ConstantBufferCache
	{
	private:
		struct ConstBuffer
		{
			Buffer				cb;
			ConstantBufferView	cbv;
			u32					size;
			int					count = 2;

			ConstBuffer(Device* pDev, u32 s)
			{
				cb.Initialize(pDev, s, 0, BufferUsage::ConstantBuffer, true, false);
				cbv.Initialize(pDev, &cb);
				size = s;
			}

			~ConstBuffer()
			{
				cbv.Destroy();
				cb.Destroy();
			}
		};	// struct ConstBuffer

	public:
		class Handle
		{
			friend class ConstantBufferCache;

		public:
			Handle()
				: pCB_(nullptr)
			{}
			Handle(const Handle&) = delete;
			Handle& operator=(const Handle&) = delete;
			Handle(Handle&& h)
			{
				Reset();

				pCache_ = h.pCache_;
				pCB_ = h.pCB_;
				h.pCache_ = nullptr;
				h.pCB_ = nullptr;
			}
			Handle& operator=(Handle&& h)
			{
				if (this == &h)
					return *this;

				Reset();

				pCache_ = h.pCache_;
				pCB_ = h.pCB_;
				h.pCache_ = nullptr;
				h.pCB_ = nullptr;
				return *this;
			}
			~Handle()
			{
				Reset();
			}

			Buffer* GetCB()
			{
				return (pCB_ != nullptr) ? &pCB_->cb : nullptr;
			}
			ConstantBufferView* GetCBV()
			{
				return (pCB_ != nullptr) ? &pCB_->cbv : nullptr;
			}

			void Reset()
			{
				if (pCache_)
				{
					pCache_->ReturnConstBuffer(pCB_);
					pCache_ = nullptr;
					pCB_ = nullptr;
				}
			}

		private:
			Handle(ConstantBufferCache* pCache, ConstBuffer* pCB)
				: pCache_(pCache), pCB_(pCB)
			{}

		private:
			ConstantBufferCache*	pCache_ = nullptr;
			ConstBuffer*			pCB_ = nullptr;
		};	// class Handle

		ConstantBufferCache()
		{}
		~ConstantBufferCache()
		{
			Destroy();
		}

		void Initialize(Device* pDevice)
		{
			pDevice_ = pDevice;
			isInitialized_ = true;
		}

		void Destroy()
		{
			for (auto&& lst : unused_)
			{
				for (auto&& b : lst.second)
					delete b;
			}
			for (auto&& b : used_)
				delete b;
			unused_.clear();
			used_.clear();
			isInitialized_ = false;
		}

		Handle GetUnusedConstBuffer(u32 size, const void* pBuffer)
		{
			static const u32 kConstantAlignment = 256;
			auto actual_size = ((size + kConstantAlignment - 1) / kConstantAlignment) * kConstantAlignment;

			auto unused_lst = unused_.find(actual_size);
			if (unused_lst == unused_.end())
			{
				unused_[actual_size] = std::list<ConstBuffer*>();
				unused_lst = unused_.find(actual_size);
			}

			ConstBuffer* ret = nullptr;
			if (!unused_lst->second.empty())
			{
				ret = *unused_lst->second.begin();
				unused_lst->second.erase(unused_lst->second.begin());
			}
			else
			{
				ret = new ConstBuffer(pDevice_, actual_size);
			}

			if (pBuffer)
			{
				memcpy(ret->cb.Map(nullptr), pBuffer, size);
				ret->cb.Unmap();
			}

			return Handle(this, ret);
		}

		void ReturnConstBuffer(ConstBuffer* p)
		{
			if (isInitialized_)
			{
				p->count = 2;
				used_.push_back(p);
			}
			else
			{
				delete p;
			}
		}

		void BeginNewFrame()
		{
			auto it = used_.begin();
			while (it != used_.end())
			{
				auto b = *it;
				b->count--;
				if (b->count == 0)
				{
					auto unused_lst = unused_.find(b->size);
					it = used_.erase(it);
					unused_lst->second.push_back(b);
				}
				else
				{
					++it;
				}
			}
		}

	private:
		Device*									pDevice_ = nullptr;
		std::map<u32, std::list<ConstBuffer*>>	unused_;
		std::list<ConstBuffer*>					used_;
		bool									isInitialized_ = false;
	};	// class ConstBufferCache

}	// namespace sl12

//	EOF
