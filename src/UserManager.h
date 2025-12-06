/**
 * @file UserManager.h
 * @brief 用户管理器和权限系统
 */
#pragma once

#include "FileHelper.h"
#include "User.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace SSDB
{
    /**
     * @brief 权限不足异常
     */
    class PermissionDeniedException : public std::runtime_error
    {
    public:
        PermissionDeniedException(std::string_view operation, Permission required);
        explicit PermissionDeniedException(const std::string& msg);
    };

    /**
     * @brief 用户管理器
     *
     * 负责用户的增删改查、权限验证等操作。
     */
    class UserManager
    {
    public:
        explicit UserManager(const std::filesystem::path& path);
        ~UserManager();

        // 禁用拷贝
        UserManager(const UserManager&) = delete;
        UserManager& operator=(const UserManager&) = delete;

        /**
         * @brief 保存用户数据到磁盘
         */
        void commit();

        // ============================================================
        // 用户认证
        // ============================================================

        /**
         * @brief 用户登录
         * @return 登录成功返回 true
         */
        bool login(const std::string& username, const std::string& password);

        /**
         * @brief 登出当前用户
         */
        void logout() noexcept;

        /**
         * @brief 检查是否已登录
         */
        [[nodiscard]] bool isLoggedIn() const noexcept;

        /**
         * @brief 获取当前登录用户
         */
        [[nodiscard]] std::optional<std::reference_wrapper<const User>> getCurrentUser() const;

        /**
         * @brief 获取当前用户 ID
         */
        [[nodiscard]] int getCurrentUserId() const noexcept;

        // ============================================================
        // 权限检查
        // ============================================================

        /**
         * @brief 检查当前用户是否有指定权限
         */
        [[nodiscard]] bool checkPermission(Permission required) const;

        /**
         * @brief 要求指定权限，没有则抛出异常
         */
        void requirePermission(Permission required, std::string_view operation = "this operation") const;

        /**
         * @brief 检查当前用户是否是 root
         */
        [[nodiscard]] bool isCurrentUserRoot() const;

        // ============================================================
        // 用户管理（需要 root 权限）
        // ============================================================

        /**
         * @brief 创建新用户（需要 root 权限）
         */
        User& createUser(const std::string& username, const std::string& password,
                        Permission perm = Permission::READ);

        /**
         * @brief 删除用户（需要 root 权限）
         */
        bool deleteUser(int userId);

        /**
         * @brief 通过用户名删除用户（需要 root 权限）
         */
        bool deleteUser(const std::string& username);

        /**
         * @brief 修改用户权限（需要 root 权限）
         */
        void setUserPermission(int userId, Permission perm);

        /**
         * @brief 修改用户密码
         * @param userId 用户ID
         * @param newPassword 新密码
         * @param oldPassword 旧密码（修改自己的密码需要验证）
         */
        void changePassword(int userId, const std::string& newPassword,
                           const std::string& oldPassword = "");

        /**
         * @brief 禁用/启用用户（需要 root 权限）
         */
        void setUserActive(int userId, bool active);

        // ============================================================
        // 用户查询
        // ============================================================

        /**
         * @brief 获取用户（通过 ID）
         */
        [[nodiscard]] std::optional<std::reference_wrapper<const User>> getUser(int userId) const;

        /**
         * @brief 获取用户（通过用户名）
         */
        [[nodiscard]] std::optional<std::reference_wrapper<const User>> getUser(const std::string& username) const;

        /**
         * @brief 检查用户是否存在（通过 ID）
         */
        [[nodiscard]] bool hasUser(int userId) const;

        /**
         * @brief 检查用户是否存在（通过用户名）
         */
        [[nodiscard]] bool hasUser(const std::string& username) const;

        /**
         * @brief 获取所有用户
         */
        [[nodiscard]] const std::unordered_map<int, User>& allUsers() const noexcept;

        /**
         * @brief 分配新用户 ID
         */
        int allocateUserId();

        // ============================================================
        // WebSocket API 支持方法（不检查权限，由调用者负责）
        // ============================================================

        /**
         * @brief 验证密码（不登录）
         */
        [[nodiscard]] bool verifyPassword(int userId, const std::string& password) const;

        /**
         * @brief 通过用户名查找用户
         */
        [[nodiscard]] std::optional<std::reference_wrapper<const User>> findUserByUsername(const std::string& username) const;

        /**
         * @brief 通过 ID 查找用户
         */
        [[nodiscard]] std::optional<std::reference_wrapper<const User>> findUserById(int userId) const;

        /**
         * @brief 获取下一个用户 ID（不分配）
         */
        [[nodiscard]] int getNextUserId() const noexcept;

        /**
         * @brief 密码哈希方法
         */
        [[nodiscard]] static std::string hashPassword(const std::string& password);

        /**
         * @brief 添加用户（不检查权限）
         */
        void addUser(const User& user);

        /**
         * @brief 删除用户（不检查权限）
         */
        bool removeUser(int userId);

        /**
         * @brief 更新用户权限（不检查权限）
         */
        void updateUserPermission(int userId, Permission perm);

        /**
         * @brief 更新用户密码（不检查权限）
         */
        void updateUserPassword(int userId, const std::string& newPassword);

        /**
         * @brief 更新用户激活状态（不检查权限）
         */
        void updateUserActive(int userId, bool active);

    private:
        DataBaseFile userDb_;
        std::unordered_map<int, User> users_;
        std::unordered_map<std::string, int> usernameIndex_;
        int maxUserId_ = 0;
        int currentUserId_ = -1;  // -1 表示未登录

        /**
         * @brief 重建用户名索引
         */
        void rebuildUsernameIndex();

        /**
         * @brief 创建默认的 root 用户
         */
        void createDefaultRootUser();
    };

} // namespace SSDB

