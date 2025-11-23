#include "FileHelper.h"

namespace SSDB
{
    DataBaseFile::DataBaseFile(fs::path path) : filePath(path)
    {
        // 确保目录存在
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
        }

        // 尝试打开文件
        File.open(path, std::ios::in | std::ios::out | std::ios::binary);

        // 如果文件不存在，File.fail() 会为真。需要创建它。
        if (!File.is_open()) {
            // 先用 out 创建
            std::ofstream create(path, std::ios::binary);
            create.close();
            // 再重新打开
            File.open(path, std::ios::in | std::ios::out | std::ios::binary);
        }

        if (!File.is_open()) {
            throw std::runtime_error("Fatal: Cannot open database file " + path.string());
        }
    }

    DataBaseFile::~DataBaseFile()
    {
        if (File.is_open()) {
            File.close();
        }
    }
}