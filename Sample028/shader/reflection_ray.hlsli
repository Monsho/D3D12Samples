#ifndef REFLECTION_RAY_HLSLI
#define REFLECTION_RAY_HLSLI

#include "math.hlsli"


// http://jcgt.org/published/0007/04/01/paper.pdf by Eric Heitz
// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
float3 SampleGGXVNDF(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
	// Section 3.2: transforming the view direction to the hemisphere configuration
	float3 Vh = normalize(float3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	float3 T1 = lensq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
	float3 T2 = cross(Vh, T1);
	// Section 4.2: parameterization of the projected area
	float r = sqrt(U1);
	float phi = 2.0 * PI * U2;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + Vh.z);
	t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
	// Section 4.3: reprojection onto hemisphere
	float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
	// Section 3.4: transforming the normal back to the ellipsoid configuration
	float3 Ne = normalize(float3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
	return Ne;
}

float3 SampleGGXVNDF_Ellipsoid(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
	return SampleGGXVNDF(Ve, alpha_x, alpha_y, U1, U2);
}

float3 SampleGGXVNDF_Hemisphere(float3 Ve, float alpha, float U1, float U2)
{
	return SampleGGXVNDF_Ellipsoid(Ve, alpha, alpha, U1, U2);
}

float3 SampleReflectionRay(float3 viewDirWS, float4 worldQuat, float roughness, float2 noiseU)
{
	float3 viewDirTS = ConvertVectorWorldToTangent(-viewDirWS, worldQuat);
	float3 sampledNormal = SampleGGXVNDF_Hemisphere(viewDirTS, roughness, noiseU.x, noiseU.y);
	float3 reflectDirTS = reflect(-viewDirTS, sampledNormal);
	return ConvertVectorTangentToWorld(reflectDirTS, worldQuat);
}

#endif
