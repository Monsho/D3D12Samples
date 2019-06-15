#pragma once

#include <sl12/util.h>
#include <sl12/command_list.h>
#include <sl12/root_signature.h>
#include <sl12/shader.h>
#include <sl12/buffer_view.h>
#include <sl12/texture_view.h>
#include <sl12/sampler.h>
#include <atomic>
#include <map>
#include <vector>


namespace sl12
{
	class DescriptorSet;

	/*************************************************//**
	 * @brief ルートシグネチャ生成記述子
	*****************************************************/
	struct RootSignatureCreateDesc
	{
		Shader*		pVS = nullptr;
		Shader*		pPS = nullptr;
		Shader*		pGS = nullptr;
		Shader*		pDS = nullptr;
		Shader*		pHS = nullptr;
		Shader*		pCS = nullptr;
	};	// struct RootSignatureCreateDesc

	/*************************************************//**
	 * @brief ルートシグネチャインスタンス
	*****************************************************/
	class RootSignatureInstance
	{
		friend class RootSignatureManager;
		friend class RootSignatureHandle;

		struct Slot
		{
			int		vs = -1;
			int		ps = -1;
			int		gs = -1;
			int		hs = -1;
			int		ds = -1;
			int		cs = -1;
		};	// struct Slot

	private:
		~RootSignatureInstance()
		{
			rootSig_.Destroy();
		}

	private:
		RootSignature					rootSig_;
		std::map<std::string, Slot>		slotMap_;
		std::atomic<int>				referenceCounter_ = 0;
		bool							isGraphics_ = true;
	};	// struct RootSignatureInstance

	/*************************************************//**
	 * @brief ルートシグネチャハンドル
	*****************************************************/
	class RootSignatureHandle
	{
		friend class RootSignatureManager;

	public:
		RootSignatureHandle()
			: pManager_(nullptr), crc_(0), pInstance_(nullptr)
		{}
		RootSignatureHandle(RootSignatureHandle& h)
			: pManager_(h.pManager_), crc_(h.crc_), pInstance_(h.pInstance_)
		{
			if (pInstance_)
			{
				pInstance_->referenceCounter_++;
			}
		}
		~RootSignatureHandle()
		{
			Invalid();
		}

		RootSignatureHandle& operator=(RootSignatureHandle& h)
		{
			pManager_ = h.pManager_;
			crc_ = h.crc_;
			pInstance_ = h.pInstance_;
			if (pInstance_)
			{
				pInstance_->referenceCounter_++;
			}
			return *this;
		}

		bool IsValid() const
		{
			return pInstance_ != nullptr;
		}

		void Invalid();

		bool SetDescriptor(DescriptorSet* pDescSet, const char* name, ConstantBufferView& cbv);
		bool SetDescriptor(DescriptorSet* pDescSet, const char* name, TextureView& srv);
		bool SetDescriptor(DescriptorSet* pDescSet, const char* name, BufferView& srv);
		bool SetDescriptor(DescriptorSet* pDescSet, const char* name, Sampler& sam);
		bool SetDescriptor(DescriptorSet* pDescSet, const char* name, UnorderedAccessView& uav);

		RootSignature* GetRootSignature()
		{
			assert(IsValid());
			return &pInstance_->rootSig_;
		}

	private:
		RootSignatureHandle(RootSignatureManager* man, u32 crc, RootSignatureInstance* ins)
			: pManager_(man), crc_(crc), pInstance_(ins)
		{
			if (pInstance_)
			{
				pInstance_->referenceCounter_++;
			}
		}

	private:
		RootSignatureManager*		pManager_ = nullptr;
		u32							crc_ = 0;
		RootSignatureInstance*		pInstance_ = nullptr;
	};	// class RootSignatureHandle

	/*************************************************//**
	 * @brief ルートシグネチャマネージャ
	*****************************************************/
	class RootSignatureManager
	{

	public:
		RootSignatureManager()
		{}
		~RootSignatureManager()
		{
			Destroy();
		}

		bool Initialize(Device* pDev);
		void Destroy();

		/**
		 * @brief ルートシグネチャを生成する
		 *
		 * 既に生成済みの場合は参照カウントをアップしてハンドルを渡す
		*/
		RootSignatureHandle CreateRootSignature(const RootSignatureCreateDesc& desc);

		/**
		 * @brief ルートシグネチャを解放する
		*/
		void ReleaseRootSignature(u32 crc, RootSignatureInstance* pInst);

	private:
		Device*									pDevice_ = nullptr;
		std::map<u32, RootSignatureInstance*>	instanceMap_;
	};
}	// namespace sl12


//	EOF
