#ifdef _DEBUG
#	pragma comment(lib, "lib/ffx_fsr2_api_x64d.lib")
#	pragma comment(lib, "lib/ffx_fsr2_api_dx12_x64d.lib")
#else
#	pragma comment(lib, "lib/ffx_fsr2_api_x64.lib")
#	pragma comment(lib, "lib/ffx_fsr2_api_dx12_x64.lib")
#endif

#include "fsr2_renderer.h"

#include "sl12/device.h"
#include "sl12/command_list.h"
#include "sl12/texture.h"


FSR2Renderer::FSR2Renderer()
{}

FSR2Renderer::~FSR2Renderer()
{
	Destroy();
}

bool FSR2Renderer::Initialize(sl12::Device* pDev, sl12::u32 renderWidth, sl12::u32 renderHeight, sl12::u32 displayWidth, sl12::u32 displayHeight, bool bInfinite, bool bHDR)
{
	context_ = std::make_unique<FfxFsr2Context>();
	description_ = std::make_unique<FfxFsr2ContextDescription>();

	const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX12();
	scratchBuffer_.reset(new char[scratchBufferSize]);
	FfxErrorCode errorCode = ffxFsr2GetInterfaceDX12(&description_->callbacks, pDev->GetDeviceDep(), scratchBuffer_.get(), scratchBufferSize);
	assert(errorCode == FFX_OK);

	description_->device = ffxGetDeviceDX12(pDev->GetDeviceDep());
	description_->maxRenderSize.width = renderWidth;
	description_->maxRenderSize.height = renderHeight;
	description_->displaySize.width = displayWidth;
	description_->displaySize.height = displayHeight;
	description_->flags = 0;
	if (bInfinite)
	{
		description_->flags |= FFX_FSR2_ENABLE_DEPTH_INFINITE;
	}
	if (bHDR)
	{
		description_->flags |= FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE;
	}

	ffxFsr2ContextCreate(context_.get(), description_.get());

	return true;
}

void FSR2Renderer::Destroy()
{
	ffxFsr2ContextDestroy(context_.get());
}

DirectX::XMFLOAT2 FSR2Renderer::BeginNewFrame(sl12::u32 renderWidth, sl12::u32 renderHeight, sl12::u32 displayWidth, sl12::u32 displayHeight)
{
	const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount((sl12::s32)renderWidth, (sl12::s32)displayWidth);
	ffxFsr2GetJitterOffset(&jitter_.x, &jitter_.y, jitterIndex_++, jitterPhaseCount);

	DirectX::XMFLOAT2 ret;
	ret.x = 2.0f * jitter_.x / (float)renderWidth;
	ret.y = 2.0f * jitter_.y / (float)renderHeight;
	return ret;
}

void FSR2Renderer::Dispatch(sl12::CommandList* pCmdList, Description* pDesc, bool bReset)
{
	assert(pDesc->pColor != nullptr);
	assert(pDesc->pDepth != nullptr);
	assert(pDesc->pMotionVector != nullptr);
	assert(pDesc->pOutput != nullptr);

	FfxFsr2DispatchDescription dispatchParameters = {};
	dispatchParameters.commandList = ffxGetCommandListDX12(pCmdList->GetLatestCommandList());
	dispatchParameters.color = ffxGetResourceDX12(context_.get(), pDesc->pColor->GetResourceDep(), nullptr);
	dispatchParameters.depth = ffxGetResourceDX12(context_.get(), pDesc->pDepth->GetResourceDep(), nullptr);
	dispatchParameters.motionVectors = ffxGetResourceDX12(context_.get(), pDesc->pMotionVector->GetResourceDep(), nullptr);
	dispatchParameters.exposure = ffxGetResourceDX12(context_.get(), pDesc->pExposure ? pDesc->pExposure->GetResourceDep() : nullptr, nullptr);
	dispatchParameters.reactive = ffxGetResourceDX12(context_.get(), pDesc->pReactiveMask ? pDesc->pReactiveMask->GetResourceDep() : nullptr, nullptr);
	dispatchParameters.transparencyAndComposition = ffxGetResourceDX12(context_.get(), pDesc->pTransparency ? pDesc->pTransparency->GetResourceDep() : nullptr, nullptr);
	dispatchParameters.output = ffxGetResourceDX12(context_.get(), pDesc->pOutput->GetResourceDep(), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
	dispatchParameters.jitterOffset.x = -jitter_.x;
	dispatchParameters.jitterOffset.y = jitter_.y;
	dispatchParameters.motionVectorScale.x = -(float)pDesc->renderWidth * 0.5f;
	dispatchParameters.motionVectorScale.y = (float)pDesc->renderHeight * 0.5f;
	dispatchParameters.reset = bReset;
	dispatchParameters.enableSharpening = pDesc->bSharpning;
	dispatchParameters.sharpness = pDesc->sharpness;
	dispatchParameters.frameTimeDelta = pDesc->deltaTime * 1000.0f;		// need milliseconds.
	dispatchParameters.preExposure = pDesc->preExposure;
	dispatchParameters.renderSize.width = pDesc->renderWidth;
	dispatchParameters.renderSize.height = pDesc->renderHeight;
	dispatchParameters.cameraFar = pDesc->cameraNear;
	dispatchParameters.cameraNear = pDesc->cameraFar;
	dispatchParameters.cameraFovAngleVertical = pDesc->cameraFovY;

	FfxErrorCode errorCode = ffxFsr2ContextDispatch(context_.get(), &dispatchParameters);
	assert(errorCode == FFX_OK);
}

//	EOF
