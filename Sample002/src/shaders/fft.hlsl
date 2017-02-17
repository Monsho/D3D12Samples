// require defines
// LENGTH : row or collumn pixel length
// BUTTERFLY_COUNT : butterfly pass count
// ROWPASS : 1 is row pass, 0 is collumn pass
// TRANSFORM_INVERSE : 1 is ifft, 0 is fft

Texture2D<float4>	inputImageR : register(t0);
Texture2D<float4>	inputImageI : register(t1);
RWTexture2D<float4>	outputImageR : register(u0);
RWTexture2D<float4>	outputImageI : register(u1);

#define PI 3.14159265

void GetButterflyValues(uint passIndex, uint x, out uint2 indices, out float2 weights)
{
	uint sectionWidth = 2 << passIndex;
	uint halfSectionWidth = sectionWidth / 2;

	uint sectionStartOffset = x & ~(sectionWidth - 1);
	uint halfSectionOffset = x & (halfSectionWidth - 1);
	uint sectionOffset = x & (sectionWidth - 1);

	float a = 2.0 * PI * float(sectionOffset) / float(sectionWidth);
	weights.y = sin(a);
	weights.x = cos(a);
	weights.y = -weights.y;

	indices.x = sectionStartOffset + halfSectionOffset;
	indices.y = sectionStartOffset + halfSectionOffset + halfSectionWidth;

	if (passIndex == 0)
	{
		indices = reversebits(indices) >> (32 - BUTTERFLY_COUNT) & (LENGTH - 1);
	}
}

groupshared float3 pingPongArray[4][LENGTH];
void ButterflyPass(uint passIndex, uint x, uint t0, uint t1, out float3 resultR, out float3 resultI)
{
	uint2 Indices;
	float2 Weights;
	GetButterflyValues(passIndex, x, Indices, Weights);

	float3 inputR1 = pingPongArray[t0][Indices.x];
	float3 inputI1 = pingPongArray[t1][Indices.x];

	float3 inputR2 = pingPongArray[t0][Indices.y];
	float3 inputI2 = pingPongArray[t1][Indices.y];

#if TRANSFORM_INVERSE
	resultR = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
	resultI = (inputI1 - Weights.y * inputR2 + Weights.x * inputI2) * 0.5;
#else
	resultR = inputR1 + Weights.x * inputR2 - Weights.y * inputI2;
	resultI = inputI1 + Weights.y * inputR2 + Weights.x * inputI2;
#endif
}

void ButterflyPassFinalNoI(uint passIndex, uint x, uint t0, uint t1, out float3 resultR)
{
	uint2 Indices;
	float2 Weights;
	GetButterflyValues(passIndex, x, Indices, Weights);

	float3 inputR1 = pingPongArray[t0][Indices.x];

	float3 inputR2 = pingPongArray[t0][Indices.y];
	float3 inputI2 = pingPongArray[t1][Indices.y];

	resultR = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
}

[numthreads(LENGTH, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
	uint2 position = dispatchID.xy;
#if ROWPASS
	uint2 texturePos = position.xy;
	uint2 outPos = texturePos;
#else
	uint2 texturePos = position.yx;
	uint2 outPos = texturePos;
#endif

	// Load entire row or column into scratch array
	float4 inputR = inputImageR.Load(int3(texturePos, 0));
	pingPongArray[0][position.x].xyz = inputR.xyz;
#if ROWPASS && !TRANSFORM_INVERSE
	// don't load values from the imaginary texture when loading the original texture
	pingPongArray[1][position.x].xyz = (0.0).xxx;
#else
	pingPongArray[1][position.x].xyz = inputImageI.Load(int3(texturePos, 0)).xyz;
#endif

	uint4 textureIndices = uint4(0, 1, 2, 3);

	for (int i = 0; i < BUTTERFLY_COUNT - 1; i++)
	{
		GroupMemoryBarrierWithGroupSync();
		ButterflyPass(i, position.x, textureIndices.x, textureIndices.y, pingPongArray[textureIndices.z][position.x].xyz, pingPongArray[textureIndices.w][position.x].xyz);
		textureIndices.xyzw = textureIndices.zwxy;
	}

	// Final butterfly will write directly to the target texture
	GroupMemoryBarrierWithGroupSync();

	// The final pass writes to the output UAV texture
#if !ROWPASS && TRANSFORM_INVERSE
	// last pass of the inverse transform. The imaginary value is no longer needed
	float3 outputR;
	ButterflyPassFinalNoI(BUTTERFLY_COUNT - 1, position.x, textureIndices.x, textureIndices.y, outputR);
	outputImageR[outPos] = float4(outputR.rgb, inputR.a);
#else
	float3 outputR, outputI;
	ButterflyPass(BUTTERFLY_COUNT - 1, position.x, textureIndices.x, textureIndices.y, outputR, outputI);
	outputImageR[outPos] = float4(outputR.rgb, inputR.a);
	outputImageI[outPos] = float4(outputI.rgb, inputR.a);
#endif
}