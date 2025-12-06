/**
 * @file User.h
 * @brief 用户实体定义
 */
#pragma once

#include "Permission.h"

#include <cstdint>
#include <string>
#include <utility>

// Cereal 序列化
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

namespace SSDB
{
    /**
     * @brief 用户实体类
     *
     * 每个用户有唯一的 ID 和用户名，以及对应的权限。
     * 支持密码验证（存储密码哈希）。
     */
    class User
    {
    private:
        int id_ = 0;
        std::string username_;
        std::string passwordHash_;
        Permission permission_ = Permission::NONE;
        bool active_ = true;

    public:
        // 默认构造函数（Cereal 反序列化需要）
        User() = default;

        // 带参构造函数
        User(int id, std::string username, std::string passwordHash, Permission perm = Permission::READ)
            : id_(id)
            , username_(std::move(username))
            , passwordHash_(std::move(passwordHash))
            , permission_(perm)
            , active_(true)
        {
        }

        // ============================================================
        // Getters / Setters
        // ============================================================

        [[nodiscard]] int GetId() const noexcept { return id_; }
        void SetId(int id) noexcept { id_ = id; }

        [[nodiscard]] const std::string& GetUsername() const noexcept { return username_; }
        void SetUsername(std::string username) { username_ = std::move(username); }

        [[nodiscard]] const std::string& GetPasswordHash() const noexcept { return passwordHash_; }
        void SetPasswordHash(std::string hash) { passwordHash_ = std::move(hash); }

        [[nodiscard]] Permission GetPermission() const noexcept { return permission_; }
        void SetPermission(Permission perm) noexcept { permission_ = perm; }

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        void SetActive(bool active) noexcept { active_ = active; }

        // ============================================================
        // 权限检查方法
        // ============================================================

        /**
         * @brief 检查用户是否拥有指定权限
         */
        [[nodiscard]] bool hasPermission(Permission required) const noexcept
        {
            return SSDB::hasPermission(permission_, required);
        }

        /**
         * @brief 检查是否为 root 用户
         */
        [[nodiscard]] bool isRoot() const noexcept
        {
            return permission_ == Permission::ROOT;
        }

        /**
         * @brief 检查是否可读
         */
        [[nodiscard]] bool canRead() const noexcept
        {
            return hasPermission(Permission::READ);
        }

        /**
         * @brief 检查是否可写
         */
        [[nodiscard]] bool canWrite() const noexcept
        {
            return hasPermission(Permission::WRITE);
        }

        /**
         * @brief 检查是否可删除
         */
        [[nodiscard]] bool canDelete() const noexcept
        {
            return hasPermission(Permission::DELETE);
        }

        // ============================================================
        // 权限修改方法
        // ============================================================

        /**
         * @brief 添加权限
         */
        void addPermission(Permission perm) noexcept
        {
            permission_ = SSDB::addPermission(permission_, perm);
        }

        /**
         * @brief 移除权限
         */
        void removePermission(Permission perm) noexcept
        {
            permission_ = SSDB::removePermission(permission_, perm);
        }

        // ============================================================
        // Cereal 序列化
        // ============================================================

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(
                cereal::make_nvp("id", id_),
                cereal::make_nvp("username", username_),
                cereal::make_nvp("passwordHash", passwordHash_),
                cereal::make_nvp("permission", reinterpret_cast<std::uint8_t&>(permission_)),
                cereal::make_nvp("active", active_)
            );
        }
    };
}

