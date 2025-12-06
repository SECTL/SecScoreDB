/**
 * @file Group.h
 * @brief 分组实体定义
 */
#pragma once

#include "SSDBType.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <string>
#include <vector>

// Cereal 序列化
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace SSDB
{
    /**
     * @brief 分组实体类
     */
    class Group
    {
    private:
        int id_ = 0;
        std::vector<int> containStudents_;
        t_metadata metadata_;

    public:
        // 默认构造函数（Cereal 反序列化需要）
        Group() = default;

        // 带参构造函数
        Group(int id, std::vector<int> students, t_metadata meta)
            : id_(id)
            , containStudents_(std::move(students))
            , metadata_(std::move(meta))
        {
        }

        // ID 访问器
        [[nodiscard]] int GetId() const noexcept { return id_; }
        void SetId(int id) noexcept { id_ = id; }

        // 学生访问器
        [[nodiscard]] const std::vector<int>& GetStudents() const noexcept { return containStudents_; }
        void SetStudents(std::vector<int> students) { containStudents_ = std::move(students); }
        void AddStudent(int studentId) { containStudents_.push_back(studentId); }

        /**
         * @brief 从分组中移除学生
         * @return 是否成功移除
         */
        bool RemoveStudent(int studentId)
        {
            if (auto it = std::ranges::find(containStudents_, studentId); it != containStudents_.end())
            {
                containStudents_.erase(it);
                return true;
            }
            return false;
        }

        /**
         * @brief 检查分组是否包含某个学生
         */
        [[nodiscard]] bool ContainsStudent(int studentId) const noexcept
        {
            return std::ranges::contains(containStudents_, studentId);
        }

        /**
         * @brief 获取分组中学生数量
         */
        [[nodiscard]] std::size_t StudentCount() const noexcept
        {
            return containStudents_.size();
        }

        // 元数据访问器
        [[nodiscard]] const t_metadata& GetMetadata() const noexcept { return metadata_; }
        void SetMetadata(t_metadata meta) { metadata_ = std::move(meta); }

        void SetMetadataValue(const std::string& key, const std::string& value)
        {
            metadata_[key] = value;
        }

        [[nodiscard]] std::string GetMetadataValue(const std::string& key) const
        {
            if (auto it = metadata_.find(key); it != metadata_.end())
            {
                return it->second;
            }
            return {};
        }

        // Cereal 序列化
        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(
                cereal::make_nvp("id", id_),
                cereal::make_nvp("students", containStudents_),
                cereal::make_nvp("metadata", metadata_)
            );
        }
    };
}
