#pragma once

#include <sl12/util.h>
#include <sl12/root_signature.h>
#include <sl12/shader.h>
#include <atomic>
#include <map>
#include <vector>


namespace sl12
{
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

	private:
		~RootSignatureInstance()
		{
			rootSig_.Destroy();
		}

	private:
		RootSignature								rootSig_;
		std::map<std::string, std::vector<int>>		slotMap_;
		std::atomic<int>							referenceCounter_ = 0;
		bool										isGraphics_ = true;
	};	// struct RootSignatureInstance

	/*************************************************//**
	 * @brief ルートシグネチャハンドル
	*****************************************************/
	class RootSignatureHandle
	{
		friend class RootSignatureManager;

	public:
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

		bool IsValid() const
		{
			return pInstance_ != nullptr;
		}

		void Invalid();

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
		RootSignatureManager*		pManager_;
		u32							crc_;
		RootSignatureInstance*		pInstance_;
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
