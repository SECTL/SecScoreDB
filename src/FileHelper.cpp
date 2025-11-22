#include "FileHelper.h"
#include<fstream>
#include <cereal/archives/binary.hpp>
#include <stdexcept>

namespace SSDB
{
    DataBaseFile::DataBaseFile(fs::path path)
    {
        this->filePath = path;
        if (!fs::exists(path))
            std::ofstream{path};
        // 移除 std::ios::app 标志以便支持随机访问
        this->File = std::fstream(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!this->File.good())
            throw std::runtime_error("there must be smth wrong with the file");
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
    
    void DataBaseFile::recordOffset(int key)
    {
        offsetTable[key] = this->File.tellp();
    }
    
    std::streampos DataBaseFile::getOffset(int key) const
    {
        auto it = offsetTable.find(key);
        if (it == offsetTable.end()) {
            throw std::out_of_range("Key not found in offset table");
        }
        return it->second;
    }
    
    bool DataBaseFile::hasKey(int key) const
    {
        return offsetTable.find(key) != offsetTable.end();
    }
    
    void DataBaseFile::removeKey(int key)
    {
        offsetTable.erase(key);
    }
    
    size_t DataBaseFile::getOffsetTableSize() const
    {
        return offsetTable.size();
    }
    
    void DataBaseFile::seek(std::streampos pos)
    {
        this->File.seekg(pos);
        this->File.seekp(pos);
    }
    
    std::streampos DataBaseFile::tell()
    {
        return this->File.tellg();
    }
    
    template<typename T>
    void DataBaseFile::writeObjAtKey(int key, const T& obj)
    {
        if (!hasKey(key)) {
            throw std::out_of_range("Key not found in offset table");
        }
        
        // 保存当前位置
        auto currentPos = this->File.tellg();
        
        // 定位到指定位置并写入对象
        seek(offsetTable[key]);
        
        std::ostringstream oss(std::ios::binary);
        cereal::BinaryOutputArchive ar(oss);
        ar(obj);
        const std::string& blob = oss.str();

        std::uint64_t len = blob.size();
        this->File.write(reinterpret_cast<const char*>(&len), sizeof(len));
        this->File.write(blob.data(), len);
        
        if (!File)
            throw std::runtime_error("Random write database Failed");
            
        // 恢复原来的位置
        this->File.seekg(currentPos);
    }
    
    template<typename T>
    bool DataBaseFile::readObjAtKey(int key, T& obj)
    {
        if (!hasKey(key)) {
            return false;
        }
        
        // 保存当前位置
        auto currentPos = this->File.tellp();
        
        // 定位到指定位置并读取对象
        seek(offsetTable[key]);
        
        std::uint64_t len = 0;
        if (!this->File.read(reinterpret_cast<char*>(&len), sizeof(len)))
            return false;
            
        std::vector<char> buf(len);
        if (!this->File.read(buf.data(), len))
            throw std::runtime_error("Random read database file error");
            
        std::istringstream iss(std::string(buf.begin(), buf.end()), std::ios::binary);
        cereal::BinaryInputArchive ar(iss);
        ar(obj);
        
        // 恢复原来的位置
        this->File.seekp(currentPos);
        return true;
    }

}