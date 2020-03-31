
// Declaration of all variables

Texture2D ocio_lut1d_0;
SamplerState ocio_lut1d_0Sampler;
Texture3D ocio_lut3d_1;
SamplerState ocio_lut3d_1Sampler;

// Declaration of all helper methods

float2 ocio_lut1d_0_computePos(float f)
{
	float dep;
	float abs_f = abs(f);
	if (abs_f > 6.10351563e-05)
	{
		float3 fComp = float3(15., 15., 15.);
		float absarr = min(abs_f, 65504.);
		fComp.x = floor(log2(absarr));
		float lower = pow(2.0, fComp.x);
		fComp.y = (absarr - lower) / lower;
		float3 scale = float3(1024., 1024., 1024.);
		dep = dot(fComp, scale);
	}
	else
	{
		dep = abs_f * 1023.0 / 6.09755516e-05;
	}
	dep += step(f, 0.0) * 32768.0;
	float2 retVal;
	retVal.y = floor(dep / 4095.);
	retVal.x = dep - retVal.y * 4095.;
	retVal.x = (retVal.x + 0.5) / 4096.;
	retVal.y = (retVal.y + 0.5) / 17.;
	return retVal;
}

// Declaration of the OCIO shader function

float4 OCIODisplay(in float4 inPixel)
{
	float4 outColor = inPixel;

	// Add a Matrix processing

	outColor = mul(outColor, float4x4(0.69545199999999996, 0.044794599999999997, -0.0055258800000000004, 0., 0.140679, 0.85967099999999996, 0.0040252100000000004, 0., 0.16386899999999999, 0.095534300000000003, 1.0015000000000001, 0., 0., 0., 0., 1.));

	// Add a LUT 1D processing for ocio_lut1d_0

	{
		outColor.r = ocio_lut1d_0.Sample(ocio_lut1d_0Sampler, ocio_lut1d_0_computePos(outColor.r)).r;
		outColor.g = ocio_lut1d_0.Sample(ocio_lut1d_0Sampler, ocio_lut1d_0_computePos(outColor.g)).g;
		outColor.b = ocio_lut1d_0.Sample(ocio_lut1d_0Sampler, ocio_lut1d_0_computePos(outColor.b)).b;
	}

	// Add a LUT 3D processing for ocio_lut3d_1

	{
		float3 coords = outColor.rgb * float3(64., 64., 64.);
		float3 baseInd = floor(coords);
		float3 frac = coords - baseInd;
		float3 f1, f4;
		baseInd = (baseInd.zyx + float3(0.5, 0.5, 0.5)) / float3(65., 65., 65.);
		float3 v1 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, baseInd).rgb;
		float3 nextInd = baseInd + float3(0.0153846154, 0.0153846154, 0.0153846154);
		float3 v4 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
		if (frac.r >= frac.g)
		{
			if (frac.g >= frac.b)
			{
				nextInd = baseInd + float3(0., 0., 0.0153846154);
				float3 v2 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				nextInd = baseInd + float3(0., 0.0153846154, 0.0153846154);
				float3 v3 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				f1 = float3(1. - frac.r, 1. - frac.r, 1. - frac.r);
				f4 = float3(frac.b, frac.b, frac.b);
				float3 f2 = float3(frac.r - frac.g, frac.r - frac.g, frac.r - frac.g);
				float3 f3 = float3(frac.g - frac.b, frac.g - frac.b, frac.g - frac.b);
				outColor.rgb = (f2 * v2) + (f3 * v3);
			}
			else if (frac.r >= frac.b)
			{
				nextInd = baseInd + float3(0., 0., 0.0153846154);
				float3 v2 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				nextInd = baseInd + float3(0.0153846154, 0., 0.0153846154);
				float3 v3 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				f1 = float3(1. - frac.r, 1. - frac.r, 1. - frac.r);
				f4 = float3(frac.g, frac.g, frac.g);
				float3 f2 = float3(frac.r - frac.b, frac.r - frac.b, frac.r - frac.b);
				float3 f3 = float3(frac.b - frac.g, frac.b - frac.g, frac.b - frac.g);
				outColor.rgb = (f2 * v2) + (f3 * v3);
			}
			else
			{
				nextInd = baseInd + float3(0.0153846154, 0., 0.);
				float3 v2 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				nextInd = baseInd + float3(0.0153846154, 0., 0.0153846154);
				float3 v3 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				f1 = float3(1. - frac.b, 1. - frac.b, 1. - frac.b);
				f4 = float3(frac.g, frac.g, frac.g);
				float3 f2 = float3(frac.b - frac.r, frac.b - frac.r, frac.b - frac.r);
				float3 f3 = float3(frac.r - frac.g, frac.r - frac.g, frac.r - frac.g);
				outColor.rgb = (f2 * v2) + (f3 * v3);
			}
		}
		else
		{
			if (frac.g <= frac.b)
			{
				nextInd = baseInd + float3(0.0153846154, 0., 0.);
				float3 v2 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				nextInd = baseInd + float3(0.0153846154, 0.0153846154, 0.);
				float3 v3 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				f1 = float3(1. - frac.b, 1. - frac.b, 1. - frac.b);
				f4 = float3(frac.r, frac.r, frac.r);
				float3 f2 = float3(frac.b - frac.g, frac.b - frac.g, frac.b - frac.g);
				float3 f3 = float3(frac.g - frac.r, frac.g - frac.r, frac.g - frac.r);
				outColor.rgb = (f2 * v2) + (f3 * v3);
			}
			else if (frac.r >= frac.b)
			{
				nextInd = baseInd + float3(0., 0.0153846154, 0.);
				float3 v2 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				nextInd = baseInd + float3(0., 0.0153846154, 0.0153846154);
				float3 v3 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				f1 = float3(1. - frac.g, 1. - frac.g, 1. - frac.g);
				f4 = float3(frac.b, frac.b, frac.b);
				float3 f2 = float3(frac.g - frac.r, frac.g - frac.r, frac.g - frac.r);
				float3 f3 = float3(frac.r - frac.b, frac.r - frac.b, frac.r - frac.b);
				outColor.rgb = (f2 * v2) + (f3 * v3);
			}
			else
			{
				nextInd = baseInd + float3(0., 0.0153846154, 0.);
				float3 v2 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				nextInd = baseInd + float3(0.0153846154, 0.0153846154, 0.);
				float3 v3 = ocio_lut3d_1.Sample(ocio_lut3d_1Sampler, nextInd).rgb;
				f1 = float3(1. - frac.g, 1. - frac.g, 1. - frac.g);
				f4 = float3(frac.r, frac.r, frac.r);
				float3 f2 = float3(frac.g - frac.b, frac.g - frac.b, frac.g - frac.b);
				float3 f3 = float3(frac.b - frac.r, frac.b - frac.r, frac.b - frac.r);
				outColor.rgb = (f2 * v2) + (f3 * v3);
			}
		}
		outColor.rgb = outColor.rgb + (f1 * v1) + (f4 * v4);
	}

	return outColor;
}
