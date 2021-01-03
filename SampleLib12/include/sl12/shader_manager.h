#pragma once

#include <sl12/util.h>
#include <sl12/shader.h>
#include <dxcapi.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>


namespace sl12
{
	inline ShaderType::Type GetShaderTypeFromFileName(const std::string& filePath)
	{
		auto tmp = filePath;
		auto pos = tmp.rfind(".hlsl");
		if (pos != std::string::npos)
		{
			tmp = tmp.erase(pos);
		}
		pos = tmp.rfind(".");
		if (pos != std::string::npos)
		{
			tmp = tmp.substr(pos + 1);
		}

		if (tmp == "vv" || tmp == "vg" || tmp == "vd") return ShaderType::Vertex;
		if (tmp == "p") return ShaderType::Pixel;
		if (tmp == "g") return ShaderType::Geometry;
		if (tmp == "d" || tmp == "dg") return ShaderType::Domain;
		if (tmp == "h") return ShaderType::Hull;
		if (tmp == "c") return ShaderType::Compute;
		if (tmp == "m") return ShaderType::Mesh;
		if (tmp == "a") return ShaderType::Amplification;
		if (tmp == "lib") return ShaderType::Library;
		return ShaderType::Max;
	}

	// shader define.
	struct ShaderDefine
	{
		std::string		name;
		std::string		value;

		ShaderDefine()
		{}
		ShaderDefine(const char* n, const char* v)
		{
			assert(n != nullptr);
			name = n;
			if (v) value = v;
		}
	};	// struct ShaderDefine

	// shader handle.
	class ShaderHandle
	{
		friend class ShaderManager;

	public:
		ShaderHandle()
		{}
		ShaderHandle(const ShaderHandle& rhs)
			: pParent_(rhs.pParent_), id_(rhs.id_)
		{}

		bool IsValid() const;

		u64 GetID() const
		{
			return id_;
		}

		const Shader* GetShader() const;
		Shader* GetShader();

	private:
		ShaderHandle(class ShaderManager* manager, u64 id)
			: pParent_(manager), id_(id)
		{}

		class ShaderManager*	pParent_ = nullptr;
		u64						id_ = 0;
	};	// class ShaderHandle

	// shader manager.
	class ShaderManager
	{
		friend class ShaderHandle;

	public:
		ShaderManager()
		{}
		~ShaderManager()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const std::vector<std::string>* includeDirs);
		void Destroy();

		bool IsCompiling() const
		{
			return !requestList_.empty() || isCompiling_;
		}

		ShaderHandle CompileFromMemory(
			const std::string& srcCode,
			const std::string& fileName,
			const std::string& entryPoint,
			ShaderType::Type type,
			int majorVersion, int minorVersion,
			const std::vector<std::string>* args,
			const std::vector<ShaderDefine>* defines);

		ShaderHandle CompileFromFile(
			const std::string& filePath,
			const std::string& entryPoint,
			ShaderType::Type type,
			int majorVersion, int minorVersion,
			const std::vector<std::string>* args,
			const std::vector<ShaderDefine>* defines);

	private:
		struct RequestItem
		{
			u64				id;
			std::wstring	filePath;
			std::string		fileDir;
			std::wstring	srcCode;
			std::wstring	fileName;
			std::wstring	entryPoint;
			std::wstring	targetProfile;
			ShaderType::Type			type;
			std::vector<std::wstring>	args;
			std::vector<ShaderDefine>	defines;
		};

		struct ShaderItem
		{
			Shader			shader;
			std::vector<u8>	binary;
		};

		ShaderHandle LoadRequest(RequestItem* item);

		ShaderItem* Compile(RequestItem* item);

		typedef std::unique_ptr<ShaderItem>	ShaderPtr;
		typedef std::unique_ptr<RequestItem> RequestItemPtr;

		Device*						pDevice_ = nullptr;
		class ShaderCompiler*		pCompiler_ = nullptr;
		std::vector<std::string>	includeDirs_;

		std::atomic<u64>			handleID_ = 0;

		std::map<u64, ShaderPtr>	shaderMap_;

		std::mutex					requestMutex_;
		std::mutex					listMutex_;
		std::condition_variable		requestCV_;
		std::thread					loadingThread_;
		std::list<RequestItemPtr>	requestList_;
		bool						isAlive_ = false;
		bool						isCompiling_ = false;
	};	// class ShaderManager

}	// namespace sl12


//	EOF
