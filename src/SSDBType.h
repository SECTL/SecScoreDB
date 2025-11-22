#pragma once
#include<map>
#include<string>
#include <filesystem>

namespace SSDB
{
	using t_metadata = std::map<std::string, std::string>;
	namespace fs = std::filesystem;
	enum class EventType
	{
		GROUP,
		STUDENT
	};
	enum class DataBaseFileType
	{
		EVENT,
		STUDENT,
		GROUP
	};
}