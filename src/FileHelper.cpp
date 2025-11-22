#include "FileHelper.h"
#include<fstream>
namespace SSDB 
{
	DataBaseFile::DataBaseFile(fs::path path)
	{
		this->filePath = path;
		if(!fs::exists(path))
			std::ofstream{ path };
		this->File = std::fstream(path, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
		if (!this->File.good())
			throw std::runtime_error("oops, seems that sth wrong had happened");
	}

	void DataBaseFile::writeBytes(const void* data, std::size_t len)
	{
		
	}
}
