#pragma once

#include <sl12/device.h>
#include <sl12/texture.h>
#include <sl12/texture_view.h>


typedef sl12::s32	ResourceID;

/************************************************//**
 * @brief 描画パスプロデューサー
****************************************************/
class PassProducer
{
public:
	virtual sl12::s32 GetInputResources(ResourceID* ids) = 0;
	virtual sl12::s32 GetOutputResources(ResourceID* ids) = 0;
	virtual void Render(sl12::CommandList* pCmdList) = 0;

protected:

};	// class PassProducer


/************************************************//**
 * @brief 描画パス
****************************************************/
class RenderPath
{
public:
	RenderPath()
	{}
	~RenderPath()
	{}

private:

};	// class RenderPathManager

//	EOF
