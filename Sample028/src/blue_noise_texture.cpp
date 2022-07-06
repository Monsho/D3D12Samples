#include "sl12/device.h"
#include "sl12/command_list.h"
#include "sl12/texture.h"
#include "sl12/buffer.h"


#include "blue_noise_data_table.h"

// from Eric Heitz's Research Page
// A Low-Discrepancy Sampler that Distributes Monte Carlo Errors as a Blue Noise in Screen Space
// https://eheitzresearch.wordpress.com/762-2/
float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(int pixel_i, int pixel_j, int sampleIndex, int sampleDimension)
{
	// wrap arguments
	pixel_i = pixel_i & 127;
	pixel_j = pixel_j & 127;
	sampleIndex = sampleIndex & 255;
	sampleDimension = sampleDimension & 255;

	// xor index based on optimized ranking
	int rankedSampleIndex = sampleIndex ^ kRankingTile[sampleDimension + (pixel_i + pixel_j*128)*8];

	// fetch value in sequence
	int value = kSobol_256spp_256d[sampleDimension + rankedSampleIndex*256];

	// If the dimension is optimized, xor sequence value based on optimized scrambling
	value = value ^ kScramblingTile[(sampleDimension%8) + (pixel_i + pixel_j*128)*8];

	// convert to float and return
	float v = (0.5f+value)/256.0f;
	return v;
}

// How to use blue noise texture.
//  const float kGoldenRatio = 1.61803398875f;
//  float2 SampleNoise = frac(BlueNoiseTexture[uv] + (frame_index & 0xff) * kGoldenRatio);
bool GenerateBlueNoiseTexture2D(sl12::Device* pDev, sl12::CommandList* pCmdList, sl12::Texture** ppTex)
{
	static const sl12::u32 kWidth = 128;
	static const DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8_UNORM;

	// generate src texture.
	std::unique_ptr<DirectX::ScratchImage> image(new DirectX::ScratchImage());
	auto hr = image->Initialize2D(kFormat, kWidth, kWidth, 1, 1);
	if (FAILED(hr))
	{
		return false;
	}
	auto dst = (sl12::u8*)image->GetPixels();
	for (sl12::u32 y = 0; y < kWidth; y++)
	{
		for (sl12::u32 x = 0; x < kWidth; x++)
		{
			dst[0] = (sl12::u8)(samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, 0) * 255.5f);
			dst[1] = (sl12::u8)(samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, 1) * 255.5f);
			dst += 2;
		}
	}

	// copy texture.
	sl12::Texture* pRet = new sl12::Texture();
	if (!pRet->InitializeFromDXImage(pDev, *image, false))
	{
		delete pRet;
		return false;
	}
	ID3D12Resource* pSrcImage = nullptr;
	if (!pRet->UpdateImage(pDev, pCmdList, *image, &pSrcImage))
	{
		delete pRet;
		return false;
	}
	pDev->PendingKill(new sl12::ReleaseObjectItem<ID3D12Resource>(pSrcImage));
	pCmdList->TransitionBarrier(pRet, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

	*ppTex = pRet;
	return true;
}


//	EOF
