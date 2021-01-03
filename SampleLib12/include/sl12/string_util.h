#pragma once

#include <sl12/util.h>
#include <string>
#include <cctype>
#include <algorithm>

namespace sl12
{

	inline std::string GetExtent(const std::string& name)
	{
		std::string ret;
		auto pos = name.rfind('.');
		if (pos == std::string::npos)
		{
			return ret;
		}

		ret = name.substr(pos);
		std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) { return std::tolower(c); });
		return ret;
	}

	inline std::string ConvertYenToSlash(const std::string& path)
	{
		std::string ret = path;
		std::transform(ret.begin(), ret.end(), ret.begin(), [](char c) { return (c == '\\') ? '/' : c; });
		return ret;
	}

	inline std::string GetFileName(const std::string& path)
	{
		std::string ret = ConvertYenToSlash(path);
		auto pos = ret.rfind('/');
		if (pos != std::string::npos)
		{
			ret = ret.substr(pos + 1);
		}
		return ret;
	}

	inline std::string GetFileNameWithoutExtent(const std::string& path)
	{
		std::string ret = GetFileName(path);
		auto pos = ret.rfind('.');
		if (pos != std::string::npos)
		{
			ret = ret.erase(pos);
		}
		return ret;
	}

	inline std::string GetFilePath(const std::string& path)
	{
		std::string ret = ConvertYenToSlash(path);
		auto pos = ret.rfind('/');
		if (pos != std::string::npos)
		{
			ret = ret.substr(0, pos + 1);
		}
		else
		{
			ret = "./";
		}
		return ret;
	}

}	// namespace sl12

	
//	EOF
