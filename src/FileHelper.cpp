/**
 * @file FileHelper.cpp
 * @brief 数据库文件操作实现
 */
#include "FileHelper.h"

#include <stdexcept>

namespace SSDB
{
    DataBaseFile::DataBaseFile(fs::path path)
        : filePath_(std::move(path))
    {
        // 确保目录存在
        if (filePath_.has_parent_path())
        {
            fs::create_directories(filePath_.parent_path());
        }

        // 尝试打开文件
        file_.open(filePath_, std::ios::in | std::ios::out | std::ios::binary);

        // 如果文件不存在，创建它
        if (!file_.is_open())
        {
            // 先用 out 创建
            std::ofstream create(filePath_, std::ios::binary);
            create.close();

            // 再重新打开
            file_.open(filePath_, std::ios::in | std::ios::out | std::ios::binary);
        }

        if (!file_.is_open())
        {
            throw std::runtime_error("Fatal: Cannot open database file " + filePath_.string());
        }
    }

    DataBaseFile::~DataBaseFile()
    {
        if (file_.is_open())
        {
            file_.close();
        }
    }
}