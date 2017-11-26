#pragma once

#include <sl12/device.h>
#include <sl12/texture.h>
#include <sl12/texture_view.h>
#include <vector>
#include <map>


typedef sl12::u32	ResourceID;
static const ResourceID		kPrevOutputID = 0x10000;
static const ResourceID		kTempResourceID = 0x10000000;

/************************************************//**
 * @brief 描画リソース記述子
 *
 * 描画リソース生成、検索時に使用する.
****************************************************/
struct RenderResourceDesc
{
	sl12::u32		width, height;				//!< バッファの幅と高さ
	float			resolution_rate;			//!< スクリーンに対する解像度の割合(0以下の場合は width, height を使用する)
	sl12::u32		mipLevels;					//!< ミップレベル(> 0)
	DXGI_FORMAT		format;						//!< フォーマット
	sl12::u32		sampleCount;				//!< サンプル数
	sl12::u32		targetCount;				//!< RTV、もしくはDSVの数(ミップレベル以下)
	sl12::u32		srvCount;					//!< SRVの数(ミップレベル+1以下)
	sl12::u32		uavCount;					//!< UAVの数(ミップレベル以下)

	RenderResourceDesc()
		: width(0), height(0)
		, resolution_rate(1.0f)
		, mipLevels(1)
		, format(DXGI_FORMAT_UNKNOWN)
		, sampleCount(1)
		, targetCount(1)
		, srvCount(1)
		, uavCount(0)
	{}

	bool operator==(const RenderResourceDesc& d) const
	{
		if (resolution_rate > 0.0f)
		{
			if (resolution_rate != d.resolution_rate) { return false; }
		}
		else
		{
			if (width != d.width || height != d.height) { return false; }
		}
		return (mipLevels == d.mipLevels)
			&& (format == d.format)
			&& (sampleCount == d.sampleCount)
			&& (targetCount == d.targetCount)
			&& (srvCount == d.srvCount)
			&& (uavCount == d.uavCount);
	}

	RenderResourceDesc& SetSize(sl12::u32 w, sl12::u32 h)
	{
		width = w;
		height = h;
		return *this;
	}
	RenderResourceDesc& SetResolutionRate(float r)
	{
		resolution_rate = r;
		return *this;
	}
	RenderResourceDesc& SetMipLevels(sl12::u32 m)
	{
		mipLevels = m;
		return *this;
	}
	RenderResourceDesc& SetFormat(DXGI_FORMAT f)
	{
		format = f;
		return *this;
	}
	RenderResourceDesc& SetSampleCount(sl12::u32 c)
	{
		sampleCount = c;
		return *this;
	}
	RenderResourceDesc& SetTargetCount(sl12::u32 c)
	{
		targetCount = c;
		return *this;
	}
	RenderResourceDesc& SetSrvCount(sl12::u32 c)
	{
		srvCount = c;
		return *this;
	}
	RenderResourceDesc& SetUavCount(sl12::u32 c)
	{
		uavCount = c;
		return *this;
	}
};	// struct RenderResourceDesc

/************************************************//**
 * @brief 描画リソース
 *
 * RTV、もしくはDSV.\n
 * テクスチャオブジェクトとRTV or DSV, SRV(, UAV)を保持する.
****************************************************/
class RenderResource
{
public:
	RenderResource()
	{}
	~RenderResource()
	{
		Destroy();
	}

	bool Initialize(sl12::Device& device, const RenderResourceDesc& desc, sl12::u32 screenWidth, sl12::u32 screenHeight);
	void Destroy();

	bool IsSameDesc(const RenderResourceDesc& d) const
	{
		return d == desc_;
	}

	bool IsRtv() const
	{
		return !rtvs_.empty();
	}

	bool IsDsv() const
	{
		return !dsvs_.empty();
	}

	bool IsUav() const
	{
		return !uavs_.empty();
	}

	//! @name 取得関数
	//! @{
	sl12::Texture* GetTexture()
	{
		return &texture_;
	}
	sl12::RenderTargetView* GetRtv(int index = 0)
	{
		return &rtvs_[index];
	}
	sl12::DepthStencilView* GetDsv(int index = 0)
	{
		return &dsvs_[index];
	}
	sl12::TextureView* GetSrv(int index = 0)
	{
		return &srvs_[index];
	}
	sl12::UnorderedAccessView* GetUav(int index = 0)
	{
		return &uavs_[index];
	}
	//! @}

private:
	RenderResourceDesc						desc_;

	sl12::Texture							texture_;
	std::vector<sl12::RenderTargetView>		rtvs_;
	std::vector<sl12::DepthStencilView>		dsvs_;
	std::vector<sl12::TextureView>			srvs_;
	std::vector<sl12::UnorderedAccessView>	uavs_;
};	// class RenderResource

/************************************************//**
 * @brief リソースプロデューサー基底
 *
 * リソースのR/Wを管理するプロデューサークラスの基底です.\n
 * インターフェースとして機能します.
****************************************************/
class ResourceProducerBase
{
public:
	//! @name 取得関数
	//! @{
	sl12::u32 GetInputCount() const
	{
		return inputCount_;
	}
	const ResourceID* GetInputIds() const
	{
		return pInputIds_;
	}
	const sl12::u32* GetInputPrevStates() const
	{
		return pInputPrevStates_;
	}

	sl12::u32 GetOutputCount() const
	{
		return outputCount_;
	}
	const ResourceID* GetOutputIds() const
	{
		return pOutputIds_;
	}
	const RenderResourceDesc* GetOutputDescs() const
	{
		return pOutputDescs_;
	}
	const sl12::u32* GetOutputPrevStates() const
	{
		return pOutputPrevStates_;
	}

	sl12::u32 GetTempCount() const
	{
		return tempCount_;
	}
	const ResourceID* GetTempIds() const
	{
		return pTempIds_;
	}
	const RenderResourceDesc* GetTempDescs() const
	{
		return pTempDescs_;
	}
	const sl12::u32* GetTempPrevStates() const
	{
		return pTempPrevStates_;
	}
	//! @}

	//! @name 設定関数
	//! @{
	void SetInput(sl12::u32 index, ResourceID id)
	{
		assert(index < inputCount_);
		pInputIds_[index] = id;
	}
	void SetInputPrevState(sl12::u32 index, sl12::u32 state)
	{
		assert(index < inputCount_);
		pInputPrevStates_[index] = state;
	}

	void SetOutput(sl12::u32 index, ResourceID id, const RenderResourceDesc& desc)
	{
		assert(index < outputCount_);
		pOutputIds_[index] = id;
		pOutputDescs_[index] = desc;
	}
	void SetOutputID(sl12::u32 index, ResourceID id)
	{
		assert(index < outputCount_);
		pOutputIds_[index] = id;
	}
	void SetOutputPrevState(sl12::u32 index, sl12::u32 state)
	{
		assert(index < outputCount_);
		pOutputPrevStates_[index] = state;
	}

	void SetTemp(sl12::u32 index, const RenderResourceDesc& desc)
	{
		assert(index < tempCount_);
		pTempDescs_[index] = desc;
	}
	void SetTempID(sl12::u32 index, ResourceID id)
	{
		assert(index < tempCount_);
		pTempIds_[index] = id;
	}
	void SetTempPrevState(sl12::u32 index, sl12::u32 state)
	{
		assert(index < tempCount_);
		pTempPrevStates_[index] = state;
	}
	//! @}

protected:
	ResourceProducerBase(
		sl12::u32 inputCount, sl12::u32 outputCount, sl12::u32 tempCount,
		ResourceID* resIds, RenderResourceDesc* resDescs, sl12::u32* pPrevStates)
		: inputCount_(inputCount), pInputIds_(resIds), pInputPrevStates_(pPrevStates)
		, outputCount_(outputCount), pOutputIds_(resIds + inputCount), pOutputDescs_(resDescs), pOutputPrevStates_(pPrevStates + inputCount)
		, tempCount_(tempCount), pTempIds_(resIds + inputCount + outputCount), pTempDescs_(resDescs + outputCount), pTempPrevStates_(pPrevStates + inputCount + outputCount)
	{}

protected:
	sl12::u32				inputCount_;
	ResourceID*				pInputIds_;
	sl12::u32*				pInputPrevStates_;

	sl12::u32				outputCount_;
	ResourceID*				pOutputIds_;
	RenderResourceDesc*		pOutputDescs_;
	sl12::u32*				pOutputPrevStates_;

	sl12::u32				tempCount_;
	ResourceID*				pTempIds_;
	RenderResourceDesc*		pTempDescs_;
	sl12::u32*				pTempPrevStates_;
};	// class ResourceProducerBase

/************************************************//**
 * @brief リソースプロデューサー
 *
 * 基底クラスで取り扱うデータを保持するテンプレートクラス.
****************************************************/
template <sl12::u32 InputCount, sl12::u32 OutputCount, sl12::u32 TempCount>
class ResourceProducer
	: public ResourceProducerBase
{
public:
	ResourceProducer()
		: ResourceProducerBase(InputCount, OutputCount, TempCount, resourceIds_, resourceDescs_, prevStates_)
	{}

private:
	ResourceID			resourceIds_[InputCount + OutputCount + TempCount];
	RenderResourceDesc	resourceDescs_[OutputCount + TempCount];
	sl12::u32			prevStates_[InputCount + OutputCount + TempCount];
};	// class ResourceProducer

/************************************************//**
 * @brief 描画リソースマネージャ
 *
 * 描画リソースの生成と管理を行う.\n
 * 基本的に、一度生成した描画リソースは削除しない.
****************************************************/
class RenderResourceManager
{
public:
	RenderResourceManager()
	{}
	~RenderResourceManager()
	{
		Destroy();
	}

	bool Initialize(sl12::Device& device, sl12::u32 screenWidth, sl12::u32 screenHeight);
	void Destroy();

	void MakeResources(std::vector<ResourceProducerBase*>& producers);

	/**
	 * @brief IDから描画リソースを取得する
	*/
	RenderResource* GetRenderResourceFromID(ResourceID id)
	{
		auto it = resource_map_.find(id);
		return (it != resource_map_.end()) ? it->second : nullptr;
	}

private:
	void AllReset();

private:
	sl12::Device*							pDevice_;
	sl12::u32								screenWidth_, screenHeight_;

	std::vector<RenderResource*>			resources_;
	std::map<ResourceID, RenderResource*>	resource_map_;
};	// class RenderResourceManager

//	EOF
