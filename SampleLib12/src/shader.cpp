#include <sl12/shader.h>

#include <sl12/device.h>
#include <sl12/file.h>


namespace sl12
{
	//----
	bool Shader::Initialize(Device* pDev, ShaderType::Type type, const char* filename)
	{
		// ファイルを読み込む
		File f;
		if (!f.ReadFile(filename))
		{
			return false;
		}

		// 初期化
		return Initialize(pDev, type, f.GetData(), f.GetSize());
	}

	//----
	bool Shader::Initialize(Device* pDev, ShaderType::Type type, const void* pData, size_t size)
	{
		if (!pData || !size)
		{
			return false;
		}

		// メモリを確保
		pData_ = new u8[size];
		if (!pData_)
		{
			return false;
		}

		// コピー
		memcpy(pData_, pData, size);

		size_ = size;
		shaderType_ = type;

		return true;
	}

	//----
	void Shader::Destroy()
	{
		sl12::SafeDeleteArray(pData_);
	}

}	// namespace sl12

//	EOF
