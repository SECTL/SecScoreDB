#pragma once
#include <string>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include "Permission.h"

namespace SSDB
{
    /**
     * @brief 用户类
     *
     * 每个用户有唯一的 id 和 username，以及对应的权限。
     * 支持密码验证（存储密码哈希）。
     */
    class User
    {
    private:
        int id;
        std::string username;
        std::string passwordHash;  // 存储密码哈希，不存明文
        Permission permission;
        bool active;               // 用户是否激活

    public:
        // 默认构造函数（cereal 反序列化需要）
        User() : id(0), permission(Permission::NONE), active(true) {}

        // 构造函数
        User(int _id, const std::string& _username, const std::string& _passwordHash, Permission _perm = Permission::READ)
            : id(_id), username(_username), passwordHash(_passwordHash), permission(_perm), active(true) {}

        // ============================================================
        // Getter / Setter
        // ============================================================

        int GetId() const { return id; }
        void SetId(int _id) { id = _id; }

        const std::string& GetUsername() const { return username; }
        void SetUsername(const std::string& _username) { username = _username; }

        const std::string& GetPasswordHash() const { return passwordHash; }
        void SetPasswordHash(const std::string& hash) { passwordHash = hash; }

        Permission GetPermission() const { return permission; }
        void SetPermission(Permission perm) { permission = perm; }

        bool IsActive() const { return active; }
        void SetActive(bool _active) { active = _active; }

        // ============================================================
        // 权限检查方法
        // ============================================================

        /**
         * @brief 检查用户是否拥有指定权限
         */
        bool hasPermission(Permission required) const
        {
            return SSDB::hasPermission(permission, required);
        }

        /**
         * @brief 检查是否为 root 用户
         */
        bool isRoot() const
        {
            return permission == Permission::ROOT;
        }

        /**
         * @brief 检查是否可读
         */
        bool canRead() const
        {
            return hasPermission(Permission::READ);
        }

        /**
         * @brief 检查是否可写
         */
        bool canWrite() const
        {
            return hasPermission(Permission::WRITE);
        }

        /**
         * @brief 检查是否可删除
         */
        bool canDelete() const
        {
            return hasPermission(Permission::DELETE);
        }

        // ============================================================
        // 权限修改方法
        // ============================================================

        /**
         * @brief 添加权限
         */
        void addPermission(Permission perm)
        {
            permission = SSDB::addPermission(permission, perm);
        }

        /**
         * @brief 移除权限
         */
        void removePermission(Permission perm)
        {
            permission = SSDB::removePermission(permission, perm);
        }

        // ============================================================
        // cereal 序列化
        // ============================================================

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(
                CEREAL_NVP(id),
                CEREAL_NVP(username),
                CEREAL_NVP(passwordHash),
                cereal::make_nvp("permission", reinterpret_cast<uint8_t&>(permission)),
                CEREAL_NVP(active)
            );
        }
    };
}

