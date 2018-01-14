// ライトのタイル処理

#include "const_buffer.hlsli"

#define kTileWidth				(16)
#define kTileSize				(kTileWidth * kTileWidth)
#define kMaxLight				(1024)

// ポイントライト座標データ
struct PointLightPos
{
	float4	posAndRadius;
};

// ポイントライトカラーデータ
struct PointLightColor
{
	float4	color;
};

// ポイントライト構造体
struct PointLight
{
	float3	pos;
	float4	color;
	float4	attn;
};

// サーフェイス情報
struct SurfaceData
{
	float3	posInWorld;
	float3	normalInWorld;
	float3	diffuseColor;
	float3	specularColor;
	float	roughness;
	float	linearDepth;
	float	viewDepth;
};

// 出力構造体
struct PointVertexGS
{
	float4	pos;
	float4	rect;			// (minX, maxX, minY, maxY)
	float	depth;
	float4	color;
	float4	attn;			// (attn, ?, ?, spec_power)
};

// 定数バッファ
cbuffer CbLightInfo
{
	uint	lightMax;
};

// 入力
StructuredBuffer<PointLightPos>		rLightPosBuffer;
StructuredBuffer<PointLightColor>	rLightColorBuffer;
Texture2D							texGBuffer0;
Texture2D							texGBuffer1;
Texture2D							texGBuffer2;
Texture2D							texLinearDepth;

// 出力
RWTexture2D<float4>					rwFinal			: register( u0 );

// 共有メモリ
groupshared uint sMinZ;		// タイルの最小深度
groupshared uint sMaxZ;		// タイルの最大深度
groupshared uint sTileLightIndices[kMaxLight];	// タイルに接触しているポイントライトのインデックス
groupshared uint sTileNumLights;				// タイルに接触しているポイントライトの数
groupshared uint sPerSamplePixels[kTileSize];
groupshared uint sNumPerSamplePixels;

// ワールド座標を取得する
float3 GetSurfaceWorldPos(uint2 uv, out float linearDepth, out float viewDepth)
{
	linearDepth = texLinearDepth[uv].r;

	float2 screen_uv = (float2)uv / screenInfo.xy;
	screen_uv = screen_uv * float2(2, -2) + float2(-1, 1);
	float3 frustumVec = { frustumCorner.x * screen_uv.x, frustumCorner.y * screen_uv.y, -frustumCorner.z };
	float3 posVS = frustumVec * linearDepth;
	viewDepth = -posVS.z;
	return mul(mtxViewToWorld, float4(posVS, 1)).xyz;
}

// サーフェイス情報を取得する
SurfaceData GetSurfaceData(uint2 uv)
{
	SurfaceData ret = (SurfaceData)0;

	ret.posInWorld = GetSurfaceWorldPos(uv, ret.linearDepth, ret.viewDepth);

	// テクスチャサンプリング
	float3 normalWS = normalize(texGBuffer0[uv].xyz);
	float3 baseColor = texGBuffer1[uv].rgb;
	float2 metalRough = texGBuffer2[uv].rg;

	ret.normalInWorld = normalize(normalWS);
	ret.diffuseColor = lerp(baseColor, (0.0).xxx, metalRough.x);
	ret.specularColor = lerp((0.0).xxx, baseColor, metalRough.x);
	ret.roughness = metalRough.y;

	return ret;
}

//! タイルの錘台を求める
void GetTileFrustumPlane( out float4 frustumPlanes[6], uint3 groupId )
{
	// タイルの最大・最小深度を浮動小数点に変換
	float minTileZ = asfloat(sMinZ);
	float maxTileZ = asfloat(sMaxZ);

	float2 tileScale = screenInfo.xy * rcp(float(2 * kTileWidth));
	float2 tileBias = tileScale - float2(groupId.xy);

	float4 c1 = float4(mtxViewToClip[0].x * tileScale.x, 0.0, -tileBias.x, 0.0);
	float4 c2 = float4(0.0, -mtxViewToClip[1].y * tileScale.y, -tileBias.y, 0.0);
	float4 c4 = float4(0.0, 0.0, -1.0, 0.0);

	frustumPlanes[0] = c4 - c1;		// Right
	frustumPlanes[1] = c1;			// Left
	frustumPlanes[2] = c4 - c2;		// Top
	frustumPlanes[3] = c2;			// Bottom
	frustumPlanes[4] = float4(0.0, 0.0, -1.0, -minTileZ);
	frustumPlanes[5] = float4(0.0, 0.0, 1.0, maxTileZ);

	// 法線が正規化されていない4面についてだけ正規化する
	[unroll]
	for (uint i = 0; i < 4; ++i)
	{
		frustumPlanes[i] *= rcp( length( frustumPlanes[i].xyz ) );
	}
}

// ポイントライトの減衰量を求める
float CalcPointLightAttn(float lengthSq, float radius)
{
	float length_attn = 1.0 / lengthSq;
	float rad_attn = 1.0 - pow(saturate(lengthSq / (radius * radius)), 10.0);
	return length_attn * rad_attn;
}

#define PI	3.1415926

// Lembert
float3 CalcDiffuseLambert(float3 C)
{
	return C / PI;
}

// GGX
float CalcD_GGX(float roughness, float NoH)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float d = (NoH * a2 - NoH) * NoH + 1;
	return a2 / (d * d * PI);
}

// Smith GGX Correlated
float CalcG_SmithGGXCorrelated(float roughness, float NoV, float NoL)
{
	float r2 = roughness * roughness;
	float v = NoL * sqrt((-NoV * r2 + NoV) * NoV + r2);
	float l = NoV * sqrt((-NoL * r2 + NoL) * NoL + r2);
	return 0.5 / (v + l);
}

// Schlick
float3 CalcF_Schlick(float3 C, float LoH)
{
	float f = pow(1 - LoH, 5);
	return f + (1 - f) * C;
}

// ポイントライトの計算を行う
float3 CalcPointLight(SurfaceData surface, PointLightPos lp, PointLightColor lc)
{
	float3 PtoL = lp.posAndRadius.xyz - surface.posInWorld;
	float lengthSq = dot(PtoL, PtoL);
	float3 L = normalize(PtoL);
	float3 N = surface.normalInWorld;
	float3 V = normalize(mtxViewToWorld._m03_m13_m23 - surface.posInWorld);
	float3 H = normalize(V + L);
	
	float NoV = abs(dot(N, V)) + 1e-5;
	float LoH = saturate(dot(L, H));
	float NoH = saturate(dot(N, H));
	float NoL = saturate(dot(N, L));

	float3 diffuse = CalcDiffuseLambert(surface.diffuseColor);
	float3 specular = CalcD_GGX(surface.roughness, NoH)
		* CalcG_SmithGGXCorrelated(surface.roughness, NoV, NoL)
		* CalcF_Schlick(surface.specularColor, LoH);

	float attn = CalcPointLightAttn(lengthSq, lp.posAndRadius.w);

	return (diffuse + specular) * NoL * attn * lc.color.rgb;
}



[numthreads(kTileWidth, kTileWidth, 1)]
void main(
	uint3 groupId          : SV_GroupID,
	uint3 dispatchThreadId : SV_DispatchThreadID,
	uint3 groupThreadId    : SV_GroupThreadID)
{
	// タイル内でのインデックスを求める
    uint groupIndex = groupThreadId.y * kTileWidth + groupThreadId.x;

	// ライトの数を取得する
	uint numLights = lightMax;

	// 各ピクセルの法線、深度、アルベドを取得する
	uint2 frameUV = dispatchThreadId.xy;
	SurfaceData surface = GetSurfaceData(frameUV);

	// 共有メモリを初期化する
	if (groupIndex == 0)
	{
		sTileNumLights = 0;
		sNumPerSamplePixels = 0;
		sMinZ = 0x7F7FFFFF;		// floatの最大値
		sMaxZ = 0;
	}

	// ここで同期を取る
    GroupMemoryBarrierWithGroupSync();

	// タイルの最大・最小深度を求める
	// この処理は並列するスレッド全てで排他的に処理される
	InterlockedMin(sMinZ, asuint(surface.viewDepth));
	InterlockedMax(sMaxZ, asuint(surface.viewDepth));

	// ここで同期を取ることでタイルの最大・最小深度を正しいものにする
    GroupMemoryBarrierWithGroupSync();

	// タイルの錘台を求める
	float4 frustumPlanes[6];
	GetTileFrustumPlane( frustumPlanes, groupId );

	// タイルとポイントライトの衝突判定
	for (uint lightIndex = groupIndex; lightIndex < numLights; lightIndex += kTileSize)
	{
		PointLightPos light = rLightPosBuffer[lightIndex];

		// View空間に座標を変換
		float3 pv = mul(mtxWorldToView, float4(light.posAndRadius.xyz, 1)).xyz;
		float radius = light.posAndRadius.w;

		// タイルとの判定
		bool inFrustum = true;
		[unroll]
		for (uint i = 0; i < 6; ++i)
		{
			float d = dot(frustumPlanes[i], float4(pv, 1));
			inFrustum = inFrustum && (d >= -radius);
		}

		// タイルと衝突している場合
		[branch]
		if (inFrustum)
		{
			uint listIndex;
			InterlockedAdd(sTileNumLights, 1, listIndex);
			sTileLightIndices[listIndex] = lightIndex;
		}
	}

	// ここで同期を取ると、sTileLightIndicesにタイルと衝突しているライトのインデックスが積まれている
    GroupMemoryBarrierWithGroupSync();

	// ライト計算
	float3 light_result = surface.diffuseColor * 0.2;
	for (uint i = 0; i < sTileNumLights; ++i)
	{
		uint lightIndex = sTileLightIndices[i];
		PointLightPos lpos = rLightPosBuffer[lightIndex];
		PointLightColor lcolor = rLightColorBuffer[lightIndex];

		light_result += CalcPointLight(surface, lpos, lcolor);
	}

	// 出力
	rwFinal[frameUV] = float4(light_result, 1.0);
}
