#include "FileHelper.h"
#include<fstream>
#include <cereal/archives/binary.hpp>

namespace SSDB
{
    DataBaseFile::DataBaseFile(fs::path path)
    {
        this->filePath = path;
        if (!fs::exists(path))
            std::ofstream{path};
        this->File = std::fstream(path, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
        if (!this->File.good())
            throw std::runtime_error("oops, seems that sth wrong had happened");
    }

    template <typename T>
    void DataBaseFile::writeObj(const T& obj)
    {
        std::ostringstream oss(std::ios::binary);
        cereal::BinaryOutputArchive ar(oss);
        ar(obj);
        const std::string& blob=oss.str();

        std::uint64_t len=blob.size();
        this->File.write(reinterpret_cast<const char*>(&len), sizeof(len));

        this->File.write(blob.data(),len);
        if (!File)
            throw std::runtime_error("write database Failed");
    }

    template <typename T>
    bool DataBaseFile::readNextObj(const T& obj)
    {
        std::uint64_t len=0;
        if (!this->File.read(reinterpret_cast<char*>(&len), sizeof(len)))
            return false;
        std::vector<char> buf(len);
        if (!this->File.read(buf.data(),len))
            throw std::runtime_error("read database file error");
        std::istringstream iss(std::string(buf.begin(), buf.end()), std::ios::binary);
        cereal::BinaryInputArchive ar(iss);
        ar(obj);
        return true;
    }
}
