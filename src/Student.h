/**
 * @file Student.h
 * @brief 学生实体定义
 */
#pragma once

#include "SSDBType.h"

#include <algorithm>
#include <concepts>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

// Cereal 序列化
#include <cereal/cereal.hpp>

namespace SSDB
{
    /**
     * @brief 元数据实体 concept
     *
     * 要求类型支持元数据的读写操作
     */
    template <typename T>
    concept MetadataEntity = requires(T t, const std::string& key, const std::string& value)
    {
        { t.SetMetadataValue(key, value) };
        { t.GetMetadataValue(key) } -> std::convertible_to<std::string>;
    };

    /**
     * @brief 学生实体类
     */
    class Student
    {
    private:
        int id_ = 0;
        std::vector<int> groupBelong_;
        t_metadata metadata_;

    public:
        // 默认构造函数（Cereal 反序列化需要）
        Student() = default;

        // 带参构造函数
        Student(int id, std::vector<int> groups, t_metadata meta)
            : id_(id)
            , groupBelong_(std::move(groups))
            , metadata_(std::move(meta))
        {
        }

        // ID 访问器
        [[nodiscard]] int GetId() const noexcept { return id_; }
        void SetId(int id) noexcept { id_ = id; }

        // 分组访问器
        [[nodiscard]] const std::vector<int>& GetGroups() const noexcept { return groupBelong_; }
        void SetGroups(std::vector<int> groups) { groupBelong_ = std::move(groups); }
        void AddGroup(int groupId) { groupBelong_.push_back(groupId); }

        /**
         * @brief 从分组中移除学生
         * @return 是否成功移除
         */
        bool RemoveGroup(int groupId)
        {
            if (auto it = std::ranges::find(groupBelong_, groupId); it != groupBelong_.end())
            {
                groupBelong_.erase(it);
                return true;
            }
            return false;
        }

        /**
         * @brief 检查是否属于某个分组
         */
        [[nodiscard]] bool BelongsToGroup(int groupId) const noexcept
        {
            return std::ranges::contains(groupBelong_, groupId);
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
                cereal::make_nvp("groups", groupBelong_),
                cereal::make_nvp("metadata", metadata_)
            );
        }
    };
}
