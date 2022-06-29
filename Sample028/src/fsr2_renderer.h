#pragma once

#include <memory>
#include <DirectXMath.h>
#include "AMD/FSR2/src/ffx_fsr2.h"
#include "AMD/FSR2/src/dx12/ffx_fsr2_dx12.h"

#include "sl12/types.h"


namespace sl12
{
	class Device;
	class CommandList;
	class Texture;
}

class FSR2Renderer
{
public:
	struct Description
	{
		sl12::Texture*		pColor = nullptr;
		sl12::Texture*		pDepth = nullptr;
		sl12::Texture*		pMotionVector = nullptr;
		sl12::Texture*		pReactiveMask = nullptr;
		sl12::Texture*		pExposure = nullptr;
		sl12::Texture*		pTransparency = nullptr;
		sl12::Texture*		pOutput = nullptr;
		sl12::u32			renderWidth, renderHeight;
		float				cameraNear, cameraFar, cameraFovY;
		float				deltaTime = 1.0f / 60.0f;
		float				preExposure = 1.0f;
		bool				bSharpning = false;
		float				sharpness = 0.0f;
	};	// struct Description

public:
	FSR2Renderer();
	~FSR2Renderer();

	bool Initialize(sl12::Device* pDev, sl12::u32 renderWidth, sl12::u32 renderHeight, sl12::u32 displayWidth, sl12::u32 displayHeight, bool bInfinite, bool bHDR);
	void Destroy();

	DirectX::XMFLOAT2 BeginNewFrame(sl12::u32 renderWidth, sl12::u32 renderHeight, sl12::u32 displayWidth, sl12::u32 displayHeight);
	void Dispatch(sl12::CommandList* pCmdList, Description* pDesc, bool bReset);

private:
	std::unique_ptr<FfxFsr2Context>				context_;
	std::unique_ptr<FfxFsr2ContextDescription>	description_;
	std::unique_ptr<char[]>						scratchBuffer_;

	sl12::u32									jitterIndex_ = 0;
	DirectX::XMFLOAT2							jitter_;
};

//	EOF
