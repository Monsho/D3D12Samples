#include <sl12/root_signature_manager.h>

#include <d3dcompiler.h>
#include <sl12/crc.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_set.h>


namespace sl12
{
	//-------------------------------------------------
	// ハンドルを無効化する
	//-------------------------------------------------
	void RootSignatureHandle::Invalid()
	{
		if (pInstance_)
		{
			pManager_->ReleaseRootSignature(crc_, pInstance_);
			pManager_ = nullptr;
			pInstance_ = nullptr;
		}
	}

	//-------------------------------------------------
	// CBVデスクリプタを設定する
	//-------------------------------------------------
	bool RootSignatureHandle::SetDescriptor(DescriptorSet* pDescSet, const char* name, ConstantBufferView& cbv)
	{
		assert(IsValid());

		auto it = pInstance_->slotMap_.find(name);
		auto isGraphics = pInstance_->isGraphics_;
		if (it != pInstance_->slotMap_.end())
		{
			if (isGraphics)
			{
				if (it->second.vs >= 0) pDescSet->SetVsCbv(it->second.vs, cbv.GetDescInfo().cpuHandle);
				if (it->second.ps >= 0) pDescSet->SetPsCbv(it->second.ps, cbv.GetDescInfo().cpuHandle);
				if (it->second.gs >= 0) pDescSet->SetGsCbv(it->second.gs, cbv.GetDescInfo().cpuHandle);
				if (it->second.hs >= 0) pDescSet->SetHsCbv(it->second.hs, cbv.GetDescInfo().cpuHandle);
				if (it->second.ds >= 0) pDescSet->SetDsCbv(it->second.ds, cbv.GetDescInfo().cpuHandle);
			}
			else
			{
				if (it->second.cs >= 0) pDescSet->SetCsCbv(it->second.cs, cbv.GetDescInfo().cpuHandle);
			}
			return true;
		}
		return false;
	}

	//-------------------------------------------------
	// テクスチャSRVデスクリプタを設定する
	//-------------------------------------------------
	bool RootSignatureHandle::SetDescriptor(DescriptorSet* pDescSet, const char* name, TextureView& srv)
	{
		assert(IsValid());

		auto it = pInstance_->slotMap_.find(name);
		auto isGraphics = pInstance_->isGraphics_;
		if (it != pInstance_->slotMap_.end())
		{
			if (isGraphics)
			{
				if (it->second.vs >= 0) pDescSet->SetVsSrv(it->second.vs, srv.GetDescInfo().cpuHandle);
				if (it->second.ps >= 0) pDescSet->SetPsSrv(it->second.ps, srv.GetDescInfo().cpuHandle);
				if (it->second.gs >= 0) pDescSet->SetGsSrv(it->second.gs, srv.GetDescInfo().cpuHandle);
				if (it->second.hs >= 0) pDescSet->SetHsSrv(it->second.hs, srv.GetDescInfo().cpuHandle);
				if (it->second.ds >= 0) pDescSet->SetDsSrv(it->second.ds, srv.GetDescInfo().cpuHandle);
			}
			else
			{
				if (it->second.cs >= 0) pDescSet->SetCsSrv(it->second.cs, srv.GetDescInfo().cpuHandle);
			}
			return true;
		}
		return false;
	}

	//-------------------------------------------------
	// バッファSRVデスクリプタを設定する
	//-------------------------------------------------
	bool RootSignatureHandle::SetDescriptor(DescriptorSet* pDescSet, const char* name, BufferView& srv)
	{
		assert(IsValid());

		auto it = pInstance_->slotMap_.find(name);
		auto isGraphics = pInstance_->isGraphics_;
		if (it != pInstance_->slotMap_.end())
		{
			if (isGraphics)
			{
				if (it->second.vs >= 0) pDescSet->SetVsSrv(it->second.vs, srv.GetDescInfo().cpuHandle);
				if (it->second.ps >= 0) pDescSet->SetPsSrv(it->second.ps, srv.GetDescInfo().cpuHandle);
				if (it->second.gs >= 0) pDescSet->SetGsSrv(it->second.gs, srv.GetDescInfo().cpuHandle);
				if (it->second.hs >= 0) pDescSet->SetHsSrv(it->second.hs, srv.GetDescInfo().cpuHandle);
				if (it->second.ds >= 0) pDescSet->SetDsSrv(it->second.ds, srv.GetDescInfo().cpuHandle);
			}
			else
			{
				if (it->second.cs >= 0) pDescSet->SetCsSrv(it->second.cs, srv.GetDescInfo().cpuHandle);
			}
			return true;
		}
		return false;
	}

	//-------------------------------------------------
	// サンプラーデスクリプタを設定する
	//-------------------------------------------------
	bool RootSignatureHandle::SetDescriptor(DescriptorSet* pDescSet, const char* name, Sampler& sam)
	{
		assert(IsValid());

		auto it = pInstance_->slotMap_.find(name);
		auto isGraphics = pInstance_->isGraphics_;
		if (it != pInstance_->slotMap_.end())
		{
			if (isGraphics)
			{
				if (it->second.vs >= 0) pDescSet->SetVsSampler(it->second.vs, sam.GetDescInfo().cpuHandle);
				if (it->second.ps >= 0) pDescSet->SetPsSampler(it->second.ps, sam.GetDescInfo().cpuHandle);
				if (it->second.gs >= 0) pDescSet->SetGsSampler(it->second.gs, sam.GetDescInfo().cpuHandle);
				if (it->second.hs >= 0) pDescSet->SetHsSampler(it->second.hs, sam.GetDescInfo().cpuHandle);
				if (it->second.ds >= 0) pDescSet->SetDsSampler(it->second.ds, sam.GetDescInfo().cpuHandle);
			}
			else
			{
				if (it->second.cs >= 0) pDescSet->SetCsSampler(it->second.cs, sam.GetDescInfo().cpuHandle);
			}
			return true;
		}
		return false;
	}

	//-------------------------------------------------
	// UAVデスクリプタを設定する
	//-------------------------------------------------
	bool RootSignatureHandle::SetDescriptor(DescriptorSet* pDescSet, const char* name, UnorderedAccessView& uav)
	{
		assert(IsValid());

		auto it = pInstance_->slotMap_.find(name);
		auto isGraphics = pInstance_->isGraphics_;
		if (it != pInstance_->slotMap_.end())
		{
			if (isGraphics)
			{
				if (it->second.ps >= 0) pDescSet->SetPsUav(it->second.ps, uav.GetDescInfo().cpuHandle);
			}
			else
			{
				if (it->second.cs >= 0) pDescSet->SetCsUav(it->second.cs, uav.GetDescInfo().cpuHandle);
			}
			return true;
		}
		return false;
	}


	//-------------------------------------------------
	// 初期化
	//-------------------------------------------------
	bool RootSignatureManager::Initialize(Device* pDev)
	{
		pDevice_ = pDev;
		return (pDev != nullptr);
	}

	//-------------------------------------------------
	// 破棄
	//-------------------------------------------------
	void RootSignatureManager::Destroy()
	{
		if (pDevice_)
		{
			for (auto&& v : instanceMap_) delete v.second;
			instanceMap_.clear();
			pDevice_ = nullptr;
		}
	}

	//-------------------------------------------------
	// ルートシグネチャを生成する
	//-------------------------------------------------
	RootSignatureHandle RootSignatureManager::CreateRootSignature(const RootSignatureCreateDesc& desc)
	{
		// シェーダからCRC32を計算する
		u32 crc;
		if (desc.pCS)
		{
			// コンピュートシェーダ
			crc = CalcCrc32(desc.pCS->GetData(), desc.pCS->GetSize());
		}
		else
		{
			// ラスタライザ
			u8 zero = 0;
			crc = desc.pVS != nullptr ? CalcCrc32(desc.pVS->GetData(), desc.pVS->GetSize()) : CalcCrc32(&zero, sizeof(zero));
			crc = desc.pPS != nullptr ? CalcCrc32(desc.pPS->GetData(), desc.pPS->GetSize(), crc) : CalcCrc32(&zero, sizeof(zero), crc);
			crc = desc.pGS != nullptr ? CalcCrc32(desc.pGS->GetData(), desc.pGS->GetSize(), crc) : CalcCrc32(&zero, sizeof(zero), crc);
			crc = desc.pDS != nullptr ? CalcCrc32(desc.pDS->GetData(), desc.pDS->GetSize(), crc) : CalcCrc32(&zero, sizeof(zero), crc);
			crc = desc.pHS != nullptr ? CalcCrc32(desc.pHS->GetData(), desc.pHS->GetSize(), crc) : CalcCrc32(&zero, sizeof(zero), crc);
		}

		// CRCから生成済みルートシグネチャを検索する
		// CRCの衝突は起きないことを祈る
		auto it = instanceMap_.find(crc);
		if (it != instanceMap_.end())
		{
			// 見つかった
			return RootSignatureHandle(this, it->first, it->second);
		}

		std::vector<RootParameter> rootParams;
		std::map<std::string, RootSignatureInstance::Slot> paramMap;
		auto SetRegIndex = [](RootSignatureInstance::Slot& slot, u32 bindPoint, u32 shaderVisibility)
		{
			switch (shaderVisibility)
			{
			case ShaderVisibility::Vertex:   slot.vs = bindPoint; break;
			case ShaderVisibility::Pixel:    slot.ps = bindPoint; break;
			case ShaderVisibility::Geometry: slot.gs = bindPoint; break;
			case ShaderVisibility::Hull:     slot.hs = bindPoint; break;
			case ShaderVisibility::Domain:   slot.ds = bindPoint; break;
			case ShaderVisibility::Compute:  slot.cs = bindPoint; break;
			}
		};
		auto ReflectShader = [&](Shader* pShader, u32 shaderVisibility)
		{
			ID3D12ShaderReflection* pReflection = nullptr;
			auto hr = D3DReflect(pShader->GetData(), pShader->GetSize(), IID_PPV_ARGS(&pReflection));
			if (FAILED(hr))
			{
				return false;
			}

			D3D12_SHADER_DESC sdesc;
			hr = pReflection->GetDesc(&sdesc);
			if (FAILED(hr))
			{
				return false;
			}

			// バインドリソースを列挙する
			for (u32 i = 0; i < sdesc.BoundResources; i++)
			{
				D3D12_SHADER_INPUT_BIND_DESC bd;
				pReflection->GetResourceBindingDesc(i, &bd);

				RootParameterType::Type paramType = RootParameterType::ConstantBuffer;
				switch (bd.Type)
				{
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
					paramType = RootParameterType::ConstantBuffer; break;
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_SAMPLER:
					paramType = RootParameterType::Sampler; break;
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_BYTEADDRESS:
					paramType = RootParameterType::ShaderResource; break;
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWBYTEADDRESS:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_APPEND_STRUCTURED:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_CONSUME_STRUCTURED:
					paramType = RootParameterType::UnorderedAccess; break;
				default:
					return false;
				}

				auto findIt = paramMap.find(bd.Name);
				if (findIt != paramMap.end())
				{
					SetRegIndex(findIt->second, bd.BindPoint, shaderVisibility);
				}
				else
				{
					RootSignatureInstance::Slot slot;
					SetRegIndex(slot, bd.BindPoint, shaderVisibility);
					paramMap[bd.Name] = slot;
				}
			}
			return true;
		};

		// 各シェーダのリソースを列挙する
		bool isGraphics = true;
		if (desc.pCS)
		{
			if (!ReflectShader(desc.pCS, ShaderVisibility::Compute))
			{
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
			isGraphics = false;
		}
		else
		{
			if (desc.pVS && !ReflectShader(desc.pVS, ShaderVisibility::Vertex))
			{
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
			if (desc.pPS && !ReflectShader(desc.pPS, ShaderVisibility::Pixel))
			{
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
			if (desc.pGS && !ReflectShader(desc.pGS, ShaderVisibility::Geometry))
			{
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
			if (desc.pDS && !ReflectShader(desc.pDS, ShaderVisibility::Domain))
			{
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
			if (desc.pHS && !ReflectShader(desc.pHS, ShaderVisibility::Hull))
			{
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
		}

		// 新規ルートシグネチャを生成する
		RootSignatureInstance* pNewInstance = new RootSignatureInstance();
		pNewInstance->isGraphics_ = isGraphics;
		pNewInstance->slotMap_ = paramMap;

		RootSignatureDesc rsDesc;
		rsDesc.numParameters = (u32)rootParams.size();
		rsDesc.pParameters = rootParams.data();
		if (isGraphics)
		{
			if (!pNewInstance->rootSig_.Initialize(pDevice_, desc.pVS, desc.pPS, desc.pGS, desc.pHS, desc.pDS))
			{
				delete pNewInstance;
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
		}
		else
		{
			if (!pNewInstance->rootSig_.Initialize(pDevice_, desc.pCS))
			{
				delete pNewInstance;
				return RootSignatureHandle(nullptr, 0, nullptr);
			}
		}

		// マップに登録
		instanceMap_[crc] = pNewInstance;

		return RootSignatureHandle(this, crc, pNewInstance);
	}

	//-------------------------------------------------
	// ルートシグネチャを解放する
	//-------------------------------------------------
	void RootSignatureManager::ReleaseRootSignature(u32 crc, RootSignatureInstance* pInst)
	{
		auto findIt = instanceMap_.find(crc);
		if (findIt != instanceMap_.end())
		{
			if (findIt->second == pInst)
			{
				pInst->referenceCounter_--;
				if (pInst->referenceCounter_ == 0)
				{
					delete pInst;
					instanceMap_.erase(findIt);
				}
			}
		}
	}

}	// namespace sl12


//	EOF
