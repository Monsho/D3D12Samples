#pragma once

#include "sl12/types.h"
#include "sl12/resource_loader.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/mesh_manager.h"

#include <DirectXMath.h>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>


namespace sl12
{

	struct ResourceMeshBoundingSphere
	{
		float	centerX, centerY, centerZ;
		float	radius;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(centerX), CEREAL_NVP(centerY), CEREAL_NVP(centerZ), CEREAL_NVP(radius));
		}
	};	// struct ResourceMeshBoundingSphere

	struct ResourceMeshBoundingBox
	{
		float	minX, minY, minZ;
		float	maxX, maxY, maxZ;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(minX), CEREAL_NVP(minY), CEREAL_NVP(minZ), CEREAL_NVP(maxX), CEREAL_NVP(maxY), CEREAL_NVP(maxZ));
		}
	};	// struct ResourceMeshBoundingBox

	struct ResourceMeshMeshletCone
	{
		float	apexX, apexY, apexZ;
		float	axisX, axisY, axisZ;
		float	cutoff;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(apexX), CEREAL_NVP(apexY), CEREAL_NVP(apexZ), CEREAL_NVP(axisX), CEREAL_NVP(axisY), CEREAL_NVP(axisZ), CEREAL_NVP(cutoff));
		}
	};	// struct ResourceMeshMeshletCone

	class ResourceMeshMaterial
	{
		friend class cereal::access;

	public:
		ResourceMeshMaterial()
		{}
		~ResourceMeshMaterial()
		{}

		const std::string& GetName() const
		{
			return name_;
		}
		const std::vector<std::string>& GetTextureNames() const
		{
			return textureNames_;
		}
		DirectX::XMFLOAT4 GetBaseColor() const
		{
			return DirectX::XMFLOAT4(baseColorR_, baseColorG_, baseColorB_, baseColorA_);
		}
		DirectX::XMFLOAT3 GetEmissiveColor() const
		{
			return DirectX::XMFLOAT3(emissiveColorR_, emissiveColorG_, emissiveColorB_);
		}
		float GetRoughness() const
		{
			return roughness_;
		}
		float GetMetallic() const
		{
			return metallic_;
		}
		bool IsOpaque() const
		{
			return isOpaque_;
		}

	private:
		std::string					name_;
		std::vector<std::string>	textureNames_;
		float						baseColorR_, baseColorG_, baseColorB_, baseColorA_;
		float						emissiveColorR_, emissiveColorG_, emissiveColorB_;
		float						roughness_;
		float						metallic_;
		bool						isOpaque_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(name_), CEREAL_NVP(textureNames_),
				CEREAL_NVP(baseColorR_), CEREAL_NVP(baseColorG_), CEREAL_NVP(baseColorB_), CEREAL_NVP(baseColorA_),
				CEREAL_NVP(emissiveColorR_), CEREAL_NVP(emissiveColorG_), CEREAL_NVP(emissiveColorB_),
				CEREAL_NVP(roughness_), CEREAL_NVP(metallic_), CEREAL_NVP(isOpaque_));
		}
	};	// class ResourceMeshMaterial

	class ResourceMeshMeshlet
	{
		friend class cereal::access;

	public:
		ResourceMeshMeshlet()
		{}
		~ResourceMeshMeshlet()
		{}

		u32 GetIndexOffset() const
		{
			return indexOffset_;
		}
		u32 GetIndexCount() const
		{
			return indexCount_;
		}
		u32 GetPrimitiveOffset() const
		{
			return primitiveOffset_;
		}
		u32 GetPrimitiveCount() const
		{
			return primitiveCount_;
		}
		u32 GetVertexIndexOffset() const
		{
			return vertexIndexOffset_;
		}
		u32 GetVertexIndexCount() const
		{
			return vertexIndexCount_;
		}
		const ResourceMeshBoundingSphere& GetBoundingSphere() const
		{
			return boundingSphere_;
		}
		const ResourceMeshBoundingBox& GetBoundingBox() const
		{
			return boundingBox_;
		}
		const ResourceMeshMeshletCone& GetCone() const
		{
			return cone_;
		}

	private:
		u32							indexOffset_;
		u32							indexCount_;
		u32							primitiveOffset_;
		u32							primitiveCount_;
		u32							vertexIndexOffset_;
		u32							vertexIndexCount_;
		ResourceMeshBoundingSphere	boundingSphere_;
		ResourceMeshBoundingBox		boundingBox_;
		ResourceMeshMeshletCone		cone_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(indexOffset_),
				CEREAL_NVP(indexCount_),
				CEREAL_NVP(primitiveOffset_),
				CEREAL_NVP(primitiveCount_),
				CEREAL_NVP(vertexIndexOffset_),
				CEREAL_NVP(vertexIndexCount_),
				CEREAL_NVP(boundingSphere_),
				CEREAL_NVP(boundingBox_),
				CEREAL_NVP(cone_));
		}
	};	// class ResourceMeshMeshlet

	class ResourceMeshSubmesh
	{
		friend class cereal::access;

	public:
		ResourceMeshSubmesh()
		{}
		~ResourceMeshSubmesh()
		{}

		int GetMaterialIndex() const
		{
			return materialIndex_;
		}
		u32 GetVertexOffset() const
		{
			return vertexOffset_;
		}
		u32 GetVertexCount() const
		{
			return vertexCount_;
		}
		u32 GetIndexOffset() const
		{
			return indexOffset_;
		}
		u32 GetIndexCount() const
		{
			return indexCount_;
		}
		u32 GetMeshletPrimitiveOffset() const
		{
			return meshletPrimitiveOffset_;
		}
		u32 GetMeshletPrimitiveCount() const
		{
			return meshletPrimitiveCount_;
		}
		u32 GetMeshletVertexIndexOffset() const
		{
			return meshletVertexIndexOffset_;
		}
		u32 GetMeshletVertexIndexCount() const
		{
			return meshletVertexIndexCount_;
		}
		const std::vector<ResourceMeshMeshlet>& GetMeshlets() const
		{
			return meshlets_;
		}
		const ResourceMeshBoundingSphere& GetBoundingSphere() const
		{
			return boundingSphere_;
		}
		const ResourceMeshBoundingBox& GetBoundingBox() const
		{
			return boundingBox_;
		}

	private:
		int									materialIndex_;
		u32									vertexOffset_;
		u32									vertexCount_;
		u32									indexOffset_;
		u32									indexCount_;
		u32									meshletPrimitiveOffset_;
		u32									meshletPrimitiveCount_;
		u32									meshletVertexIndexOffset_;
		u32									meshletVertexIndexCount_;
		std::vector<ResourceMeshMeshlet>	meshlets_;
		ResourceMeshBoundingSphere			boundingSphere_;
		ResourceMeshBoundingBox				boundingBox_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(materialIndex_),
				CEREAL_NVP(vertexOffset_),
				CEREAL_NVP(vertexCount_),
				CEREAL_NVP(indexOffset_),
				CEREAL_NVP(indexCount_),
				CEREAL_NVP(meshletPrimitiveOffset_),
				CEREAL_NVP(meshletPrimitiveCount_),
				CEREAL_NVP(meshletVertexIndexOffset_),
				CEREAL_NVP(meshletVertexIndexCount_),
				CEREAL_NVP(meshlets_),
				CEREAL_NVP(boundingSphere_),
				CEREAL_NVP(boundingBox_));
		}
	};	// class ResourceMeshSubmesh

	class ResourceMesh
	{
		friend class cereal::access;

	public:
		ResourceMesh()
		{}
		~ResourceMesh()
		{}

		const std::vector<ResourceMeshMaterial>& GetMaterials() const
		{
			return materials_;
		}
		const std::vector<ResourceMeshSubmesh>& GetSubmeshes() const
		{
			return submeshes_;
		}
		const ResourceMeshBoundingSphere& GetBoundingSphere() const
		{
			return boundingSphere_;
		}
		const ResourceMeshBoundingBox& GetBoundingBox() const
		{
			return boundingBox_;
		}

		const std::vector<u8>& GetVBPosition() const
		{
			return vbPosition_;
		}
		const std::vector<u8>& GetVBNormal() const
		{
			return vbNormal_;
		}
		const std::vector<u8>& GetVBTangent() const
		{
			return vbTangent_;
		}
		const std::vector<u8>& GetVBTexcoord() const
		{
			return vbTexcoord_;
		}
		const std::vector<u8>& GetIndexBuffer() const
		{
			return indexBuffer_;
		}
		const std::vector<u8>& GetMeshletPackedPrimitive() const
		{
			return meshletPackedPrimitive_;
		}
		const std::vector<u8>& GetMeshletVertexIndex() const
		{
			return meshletVertexIndex_;
		}

	private:
		std::vector<ResourceMeshMaterial>	materials_;
		std::vector<ResourceMeshSubmesh>	submeshes_;
		ResourceMeshBoundingSphere			boundingSphere_;
		ResourceMeshBoundingBox				boundingBox_;

		std::vector<u8>						vbPosition_;
		std::vector<u8>						vbNormal_;
		std::vector<u8>						vbTangent_;
		std::vector<u8>						vbTexcoord_;
		std::vector<u8>						indexBuffer_;
		std::vector<u8>						meshletPackedPrimitive_;
		std::vector<u8>						meshletVertexIndex_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(materials_),
				CEREAL_NVP(submeshes_),
				CEREAL_NVP(boundingSphere_),
				CEREAL_NVP(boundingBox_),
				CEREAL_NVP(vbPosition_),
				CEREAL_NVP(vbNormal_),
				CEREAL_NVP(vbTangent_),
				CEREAL_NVP(vbTexcoord_),
				CEREAL_NVP(indexBuffer_),
				CEREAL_NVP(meshletPackedPrimitive_),
				CEREAL_NVP(meshletVertexIndex_));
		}
	};	// class ResourceMesh

	class ResourceItemMesh
		: public ResourceItemBase
	{
	public:
		struct Bounding
		{
			struct
			{
				DirectX::XMFLOAT3	center;
				float				radius;
			} sphere;
			struct
			{
				DirectX::XMFLOAT3	aabbMin, aabbMax;
			} box;
			struct
			{
				DirectX::XMFLOAT3	apex;
				DirectX::XMFLOAT3	axis;
				float				cutoff;
			} cone;
		};	// struct Bounding

		struct Material
		{
			std::string			name;
			ResourceHandle		baseColorTex;
			ResourceHandle		normalTex;
			ResourceHandle		ormTex;
			DirectX::XMFLOAT4	baseColor;
			DirectX::XMFLOAT3	emissiveColor;
			float				roughness;
			float				metallic;
			bool				isOpaque;
		};	// struct Material

		struct Meshlet
		{
			u32			indexOffset;
			u32			indexCount;
			u32			primitiveOffset;
			u32			primitiveCount;
			u32			vertexIndexOffset;
			u32			vertexIndexCount;
			Bounding	boundingInfo;
		};	// struct Meshlet

		struct Submesh
		{
			int					materialIndex;
			u32					vertexCount;
			u32					indexCount;
			size_t				positionSizeBytes;
			size_t				normalSizeBytes;
			size_t				tangentSizeBytes;
			size_t				texcoordSizeBytes;
			size_t				indexSizeBytes;
			size_t				positionOffsetBytes;
			size_t				normalOffsetBytes;
			size_t				tangentOffsetBytes;
			size_t				texcoordOffsetBytes;
			size_t				indexOffsetBytes;
			VertexBufferView	positionVBV;
			VertexBufferView	normalVBV;
			VertexBufferView	tangentVBV;
			VertexBufferView	texcoordVBV;
			IndexBufferView		indexBV;
			BufferView			positionView;
			BufferView			normalView;
			BufferView			tangentView;
			BufferView			texcoordView;
			BufferView			indexView;
			BufferView			packedPrimitiveView;
			BufferView			vertexIndexView;
			Bounding			boundingInfo;

			std::vector<Meshlet>	meshlets;
		};	// struct Submesh

	public:
		static const u32 kType = TYPE_FOURCC("MESH");

		~ResourceItemMesh();

		const std::vector<Material>& GetMaterials() const
		{
			return mateirals_;
		}
		const std::vector<Submesh>& GetSubmeshes() const
		{
			return Submeshes_;
		}
		const Bounding& GetBoundingInfo() const
		{
			return boundingInfo_;
		}
		const Buffer& GetPositionVB() const
		{
			return positionVB_;
		}
		const Buffer& GetNormalVB() const
		{
			return normalVB_;
		}
		const Buffer& GetTangentVB() const
		{
			return tangentVB_;
		}
		const Buffer& GetTexcoordVB() const
		{
			return texcoordVB_;
		}
		const Buffer& GetIndexBuffer() const
		{
			return indexBuffer_;
		}
		const Buffer& GetMeshletPackedPrimitive() const
		{
			return meshletPackedPrimitive_;
		}
		const Buffer& GetMeshletVertexIndex() const
		{
			return meshletVertexIndex_;
		}

		Buffer& GetPositionVB()
		{
			return positionVB_;
		}
		Buffer& GetNormalVB()
		{
			return normalVB_;
		}
		Buffer& GetTangentVB()
		{
			return tangentVB_;
		}
		Buffer& GetTexcoordVB()
		{
			return texcoordVB_;
		}
		Buffer& GetIndexBuffer()
		{
			return indexBuffer_;
		}
		Buffer& GetMeshletPackedPrimitive()
		{
			return meshletPackedPrimitive_;
		}
		Buffer& GetMeshletVertexIndex()
		{
			return meshletVertexIndex_;
		}

		const MeshManager::Handle& GetPositionHandle() const
		{
			return hPosition_;
		}
		const MeshManager::Handle& GetNormalHandle() const
		{
			return hNormal_;
		}
		const MeshManager::Handle& GetTangentHandle() const
		{
			return hTangent_;
		}
		const MeshManager::Handle& GetTexcoordHandle() const
		{
			return hTexcoord_;
		}
		const MeshManager::Handle& GetIndexHandle() const
		{
			return hIndex_;
		}

		static ResourceItemBase* LoadFunction(ResourceLoader* pLoader, const std::string& filepath);

		static size_t GetPositionStride()
		{
			return sizeof(DirectX::XMFLOAT3);
		}
		static size_t GetNormalStride()
		{
			return sizeof(DirectX::XMFLOAT3);
		}
		static size_t GetTangentStride()
		{
			return sizeof(DirectX::XMFLOAT4);
		}
		static size_t GetTexcoordStride()
		{
			return sizeof(DirectX::XMFLOAT2);
		}
		static size_t GetIndexStride()
		{
			return sizeof(u32);
		}

	private:
		ResourceItemMesh()
			: ResourceItemBase(ResourceItemMesh::kType)
		{}

	private:
		std::vector<Material>	mateirals_;
		std::vector<Submesh>	Submeshes_;
		Bounding				boundingInfo_;

		Buffer		positionVB_;
		Buffer		normalVB_;
		Buffer		tangentVB_;
		Buffer		texcoordVB_;
		Buffer		indexBuffer_;
		Buffer		meshletPackedPrimitive_;
		Buffer		meshletVertexIndex_;

		MeshManager::Handle	hPosition_;
		MeshManager::Handle	hNormal_;
		MeshManager::Handle	hTangent_;
		MeshManager::Handle	hTexcoord_;
		MeshManager::Handle	hIndex_;
	};	// class ResourceItemMesh

}	// namespace sl12


//	EOF
