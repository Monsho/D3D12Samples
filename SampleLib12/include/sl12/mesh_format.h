#pragma once

#include "types.h"

namespace sl12
{
	/**********************************************//**
	 * @brief シェイプ
	**************************************************/
	struct MeshShape
	{
		char	name[64];
		u32		numVertices;
		u32		numIndices;
		u64		positionOffset;
		u64		normalOffset;
		u64		texcoordOffset;
	};	// struct MeshShape

	/**********************************************//**
	 * @brief マテリアル
	**************************************************/
	struct MeshMaterial
	{
		char	name[64];
	};	// struct MeshMaterial

	/**********************************************//**
	 * @brief サブメッシュ
	**************************************************/
	struct MeshSubmesh
	{
		s32		shapeIndex;
		s32		materialIndex;
		u64		indexBufferOffset;
		u32		numSubmeshIndices;
	};	// struct MeshMaterial

	/**********************************************//**
	 * @brief メッシュヘッダ
	**************************************************/
	struct MeshHead
	{
		char	fourCC[4];
		s32		numShapes;
		s32		numMaterials;
		s32		numSubmeshes;
	};	// struct MeshHead

}	// namespace sl12


//	EOF
