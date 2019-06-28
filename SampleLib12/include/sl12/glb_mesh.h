#pragma once

#include <vector>
#include <memory>

#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/texture.h"
#include "sl12/texture_view.h"


namespace Microsoft
{
	namespace glTF
	{
		class Document;
		struct MeshPrimitive;
		struct Material;
		class GLTFResourceReader;
	}
}

namespace sl12
{
	class GlbSubmesh
	{
		friend class GlbMesh;

		template <typename View>
		struct BufferBundle
		{
			Buffer					buffer_;
			BufferView				buffer_view_;
			View					view_;
			std::unique_ptr<u8[]>	source_data_;

			~BufferBundle()
			{
				view_.Destroy();
				buffer_view_.Destroy();
				buffer_.Destroy();
				source_data_.release();
			}
		};	// struct BufferBundle

	public:
		GlbSubmesh()
		{}
		~GlbSubmesh()
		{
			Destroy();
		}

		// getter
		Buffer& GetPositionB()
		{
			return positionBuffer_.buffer_;
		}
		BufferView& GetPositionBV()
		{
			return positionBuffer_.buffer_view_;
		}
		VertexBufferView& GetPositionVBV()
		{
			return positionBuffer_.view_;
		}
		const DirectX::XMFLOAT3* GetPositionSourceData()
		{
			return reinterpret_cast<DirectX::XMFLOAT3*>(positionBuffer_.source_data_.get());
		}

		Buffer& GetNormalB()
		{
			return normalBuffer_.buffer_;
		}
		BufferView& GetNormalBV()
		{
			return normalBuffer_.buffer_view_;
		}
		VertexBufferView& GetNormalVBV()
		{
			return normalBuffer_.view_;
		}
		const DirectX::XMFLOAT3* GetNormalSourceData()
		{
			return reinterpret_cast<DirectX::XMFLOAT3*>(normalBuffer_.source_data_.get());
		}

		Buffer& GetTexcoordB()
		{
			return texcoordBuffer_.buffer_;
		}
		BufferView& GetTexcoordBV()
		{
			return texcoordBuffer_.buffer_view_;
		}
		VertexBufferView& GetTexcoordVBV()
		{
			return texcoordBuffer_.view_;
		}
		const DirectX::XMFLOAT2* GetTexcoordSourceData()
		{
			return reinterpret_cast<DirectX::XMFLOAT2*>(texcoordBuffer_.source_data_.get());
		}

		Buffer& GetUniqueUvB()
		{
			return uniqueUvBuffer_.buffer_;
		}
		BufferView& GetUniqueUvBV()
		{
			return uniqueUvBuffer_.buffer_view_;
		}
		VertexBufferView& GetUniqueUvVBV()
		{
			return uniqueUvBuffer_.view_;
		}
		const DirectX::XMFLOAT2* GetUniqueUvSourceData()
		{
			return reinterpret_cast<DirectX::XMFLOAT2*>(uniqueUvBuffer_.source_data_.get());
		}

		Buffer& GetIndexB()
		{
			return indexBuffer_.buffer_;
		}
		BufferView& GetIndexBV()
		{
			return indexBuffer_.buffer_view_;
		}
		IndexBufferView& GetIndexIBV()
		{
			return indexBuffer_.view_;
		}
		const u32* GetIndexSourceData()
		{
			return reinterpret_cast<u32*>(indexBuffer_.source_data_.get());
		}

		int GetMaterialIndex() const
		{
			return materialIndex_;
		}

		int GetVerticesCount() const
		{
			return verticesCount_;
		}

		int GetIndicesCount() const
		{
			return indicesCount_;
		}

	private:
		bool Initialize(Device* pDev, const Microsoft::glTF::Document& doc, const Microsoft::glTF::MeshPrimitive& mesh, Microsoft::glTF::GLTFResourceReader& resReader);
		void Destroy();

		bool CreateBuffers(Device* pDev);

	private:
		BufferBundle<VertexBufferView>	positionBuffer_;
		BufferBundle<VertexBufferView>	normalBuffer_;
		BufferBundle<VertexBufferView>	texcoordBuffer_;
		BufferBundle<VertexBufferView>	uniqueUvBuffer_;
		BufferBundle<IndexBufferView>	indexBuffer_;

		int								materialIndex_ = -1;
		int								verticesCount_ = 0;
		int								indicesCount_ = 0;
	};	// class GlbSubmesh

	class GlbMaterial
	{
		friend class GlbMesh;

	public:
		struct BlendMode
		{
			enum Value
			{
				Opaque,
				Blend,
				Mask,

				Max
			};
		};

	public:
		GlbMaterial()
		{}
		~GlbMaterial()
		{
			Destroy();
		}

		// getter
		const std::string& GetName() const
		{
			return name_;
		}
		int GetTexBaseColorIndex() const
		{
			return texBaseColorIndex_;
		}
		int GetTexMetalRoughIndex() const
		{
			return texMetalRoughIndex_;
		}
		int GetTexNormalIndex() const
		{
			return texNormalIndex_;
		}
		BlendMode::Value GetBlendMode() const
		{
			return blendMode_;
		}

	private:
		bool Initialize(Device* pDev, const Microsoft::glTF::Document& doc, const Microsoft::glTF::Material& mat, Microsoft::glTF::GLTFResourceReader& resReader);
		void Destroy();

	private:
		std::string			name_;
		int					texBaseColorIndex_ = -1;
		int					texMetalRoughIndex_ = -1;
		int					texNormalIndex_ = -1;
		BlendMode::Value	blendMode_ = BlendMode::Opaque;
	};	// class GlbMaterial

	class GlbMesh
	{
		struct TextureSet
		{
			Texture*			pTex = nullptr;
			TextureView*		pView = nullptr;

			~TextureSet()
			{
				delete pView;
				delete pTex;
			}
		};	// struct TextureSet

	public:
		GlbMesh()
		{}
		~GlbMesh()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, CommandList* pCmdList, const char* pathname, const char* filename);
		void Destroy();

		bool GenerateAtlas(Device* pDev, size_t texWidth, size_t texHeight, bool use_uv_for_imt, int options);

		// getter
		int GetSubmeshCount() const
		{
			return (int)submeshes_.size();
		}
		std::shared_ptr<GlbSubmesh> GetSubmesh(int index)
		{
			return submeshes_[index];
		}
		std::shared_ptr<GlbMaterial> GetMaterial(int index)
		{
			return materials_[index];
		}
		TextureView* GetTextureView(int index)
		{
			return textures_[index].pView;
		}

	private:
		std::vector<std::shared_ptr<GlbSubmesh>>	submeshes_;
		std::vector<std::shared_ptr<GlbMaterial>>	materials_;
		std::vector<TextureSet>						textures_;
	};	// class GlbMesh

}	// namespace sl12


//	EOF
