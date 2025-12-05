#pragma once
#include "User.h"
#include "FileHelper.h"
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <format>
#include <functional>

namespace SSDB
{
    /**
     * @brief 权限不足异常
     */
    class PermissionDeniedException : public std::runtime_error
    {
    public:
        PermissionDeniedException(const std::string& operation, Permission required)
            : std::runtime_error(
                "Permission denied: Operation '" + operation + "' requires " +
                permissionToString(required) + " permission.") {}

        PermissionDeniedException(const std::string& msg)
            : std::runtime_error(msg) {}
    };

    /**
     * @brief 用户管理器
     *
     * 负责用户的增删改查、权限验证等操作。
     */
    class UserManager
    {
    private:
        DataBaseFile user_db;
        std::unordered_map<int, User> users;
        std::unordered_map<std::string, int> usernameIndex;  // username -> id 的索引
        int _max_user_id = 0;
        int _current_user_id = -1;  // 当前登录用户的 ID，-1 表示未登录

        /**
         * @brief 重建用户名索引
         */
        void rebuildUsernameIndex()
        {
            usernameIndex.clear();
            for (const auto& [id, user] : users)
            {
                usernameIndex[user.GetUsername()] = id;
            }
        }

        /**
         * @brief 简单的密码哈希函数（生产环境应使用 bcrypt 等）
         */
        static std::string simpleHash(const std::string& password)
        {
            // 简单的哈希实现，生产环境请使用更安全的算法
            std::hash<std::string> hasher;
            return std::to_string(hasher(password + "SSDB_SALT_2024"));
        }

    public:
        explicit UserManager(const std::filesystem::path& path)
            : user_db(path / "users.bin")
        {
            users = user_db.LoadAll<User>();
            rebuildUsernameIndex();

            // 初始化最大 ID
            for (const auto& [id, _] : users)
            {
                if (id > _max_user_id) _max_user_id = id;
            }

            // 如果没有用户，创建默认的 root 用户
            if (users.empty())
            {
                createDefaultRootUser();
            }
        }

        ~UserManager()
        {
            try
            {
                commit();
            }
            catch (const std::exception& e)
            {
                std::cerr << "[UserManager Error] Failed to save users: " << e.what() << std::endl;
            }
        }

        /**
         * @brief 保存用户数据
         */
        void commit()
        {
            user_db.SaveAll(users);
        }

        // ============================================================
        // 用户认证
        // ============================================================

        /**
         * @brief 用户登录
         * @return 登录成功返回 true
         */
        bool login(const std::string& username, const std::string& password)
        {
            auto it = usernameIndex.find(username);
            if (it == usernameIndex.end()) return false;

            const User& user = users.at(it->second);
            if (!user.IsActive()) return false;

            if (user.GetPasswordHash() == simpleHash(password))
            {
                _current_user_id = user.GetId();
                return true;
            }
            return false;
        }

        /**
         * @brief 登出当前用户
         */
        void logout()
        {
            _current_user_id = -1;
        }

        /**
         * @brief 检查是否已登录
         */
        bool isLoggedIn() const
        {
            return _current_user_id != -1;
        }

        /**
         * @brief 获取当前登录用户
         */
        std::optional<std::reference_wrapper<const User>> getCurrentUser() const
        {
            if (_current_user_id == -1) return std::nullopt;
            auto it = users.find(_current_user_id);
            if (it == users.end()) return std::nullopt;
            return std::cref(it->second);
        }

        /**
         * @brief 获取当前用户 ID
         */
        int getCurrentUserId() const
        {
            return _current_user_id;
        }

        // ============================================================
        // 权限检查
        // ============================================================

        /**
         * @brief 检查当前用户是否有指定权限
         */
        bool checkPermission(Permission required) const
        {
            if (_current_user_id == -1) return false;
            auto it = users.find(_current_user_id);
            if (it == users.end()) return false;
            return it->second.hasPermission(required);
        }

        /**
         * @brief 要求指定权限，没有则抛出异常
         */
        void requirePermission(Permission required, const std::string& operation = "this operation") const
        {
            if (!checkPermission(required))
            {
                throw PermissionDeniedException(operation, required);
            }
        }

        /**
         * @brief 检查当前用户是否是 root
         */
        bool isCurrentUserRoot() const
        {
            if (_current_user_id == -1) return false;
            auto it = users.find(_current_user_id);
            if (it == users.end()) return false;
            return it->second.isRoot();
        }

        // ============================================================
        // 用户管理（需要 root 权限）
        // ============================================================

        /**
         * @brief 创建新用户（需要 root 权限）
         */
        User& createUser(const std::string& username, const std::string& password, Permission perm = Permission::READ)
        {
            // 检查权限
            if (!isCurrentUserRoot())
            {
                throw PermissionDeniedException("Only root user can create new users.");
            }

            // 检查用户名是否已存在
            if (usernameIndex.contains(username))
            {
                throw std::runtime_error("Username '" + username + "' already exists.");
            }

            int newId = ++_max_user_id;
            User newUser(newId, username, simpleHash(password), perm);

            auto [it, success] = users.emplace(newId, std::move(newUser));
            usernameIndex[username] = newId;

            return it->second;
        }

        /**
         * @brief 删除用户（需要 root 权限）
         */
        bool deleteUser(int userId)
        {
            if (!isCurrentUserRoot())
            {
                throw PermissionDeniedException("Only root user can delete users.");
            }

            // 不能删除自己
            if (userId == _current_user_id)
            {
                throw std::runtime_error("Cannot delete the currently logged-in user.");
            }

            auto it = users.find(userId);
            if (it == users.end()) return false;

            usernameIndex.erase(it->second.GetUsername());
            users.erase(it);
            return true;
        }

        /**
         * @brief 通过用户名删除用户
         */
        bool deleteUser(const std::string& username)
        {
            auto it = usernameIndex.find(username);
            if (it == usernameIndex.end()) return false;
            return deleteUser(it->second);
        }

        /**
         * @brief 修改用户权限（需要 root 权限）
         */
        void setUserPermission(int userId, Permission perm)
        {
            if (!isCurrentUserRoot())
            {
                throw PermissionDeniedException("Only root user can modify permissions.");
            }

            auto it = users.find(userId);
            if (it == users.end())
            {
                throw std::runtime_error("User ID " + std::to_string(userId) + " not found.");
            }

            it->second.SetPermission(perm);
        }

        /**
         * @brief 修改用户密码
         * @param userId 用户ID
         * @param newPassword 新密码
         * @param oldPassword 旧密码（如果修改自己的密码需要验证）
         */
        void changePassword(int userId, const std::string& newPassword, const std::string& oldPassword = "")
        {
            auto it = users.find(userId);
            if (it == users.end())
            {
                throw std::runtime_error("User ID " + std::to_string(userId) + " not found.");
            }

            // 修改自己的密码需要验证旧密码
            if (userId == _current_user_id)
            {
                if (it->second.GetPasswordHash() != simpleHash(oldPassword))
                {
                    throw std::runtime_error("Old password is incorrect.");
                }
            }
            // 修改其他人的密码需要 root 权限
            else if (!isCurrentUserRoot())
            {
                throw PermissionDeniedException("Only root user can change other users' passwords.");
            }

            it->second.SetPasswordHash(simpleHash(newPassword));
        }

        /**
         * @brief 禁用/启用用户（需要 root 权限）
         */
        void setUserActive(int userId, bool active)
        {
            if (!isCurrentUserRoot())
            {
                throw PermissionDeniedException("Only root user can enable/disable users.");
            }

            if (userId == _current_user_id && !active)
            {
                throw std::runtime_error("Cannot disable the currently logged-in user.");
            }

            auto it = users.find(userId);
            if (it == users.end())
            {
                throw std::runtime_error("User ID " + std::to_string(userId) + " not found.");
            }

            it->second.SetActive(active);
        }

        // ============================================================
        // 用户查询
        // ============================================================

        /**
         * @brief 获取用户（通过 ID）
         */
        std::optional<std::reference_wrapper<const User>> getUser(int userId) const
        {
            auto it = users.find(userId);
            if (it == users.end()) return std::nullopt;
            return std::cref(it->second);
        }

        /**
         * @brief 获取用户（通过用户名）
         */
        std::optional<std::reference_wrapper<const User>> getUser(const std::string& username) const
        {
            auto it = usernameIndex.find(username);
            if (it == usernameIndex.end()) return std::nullopt;
            return getUser(it->second);
        }

        /**
         * @brief 检查用户是否存在
         */
        bool hasUser(int userId) const
        {
            return users.contains(userId);
        }

        /**
         * @brief 检查用户名是否存在
         */
        bool hasUser(const std::string& username) const
        {
            return usernameIndex.contains(username);
        }

        /**
         * @brief 获取所有用户
         */
        const std::unordered_map<int, User>& allUsers() const
        {
            return users;
        }

        /**
         * @brief 分配新用户 ID
         */
        int allocateUserId()
        {
            return ++_max_user_id;
        }

        // ============================================================
        // WebSocket API 支持方法
        // ============================================================

        /**
         * @brief 验证密码（不登录）
         */
        bool verifyPassword(int userId, const std::string& password) const
        {
            auto it = users.find(userId);
            if (it == users.end()) return false;
            return it->second.GetPasswordHash() == simpleHash(password);
        }

        /**
         * @brief 通过用户名查找用户
         */
        std::optional<std::reference_wrapper<const User>> findUserByUsername(const std::string& username) const
        {
            auto it = usernameIndex.find(username);
            if (it == usernameIndex.end()) return std::nullopt;
            auto userIt = users.find(it->second);
            if (userIt == users.end()) return std::nullopt;
            return std::cref(userIt->second);
        }

        /**
         * @brief 通过 ID 查找用户
         */
        std::optional<std::reference_wrapper<const User>> findUserById(int userId) const
        {
            auto it = users.find(userId);
            if (it == users.end()) return std::nullopt;
            return std::cref(it->second);
        }

        /**
         * @brief 获取下一个用户 ID（不分配）
         */
        int getNextUserId() const
        {
            return _max_user_id + 1;
        }

        /**
         * @brief 密码哈希公开方法（用于创建用户）
         */
        std::string hashPassword(const std::string& password) const
        {
            return simpleHash(password);
        }

        /**
         * @brief 添加用户（不检查权限，由调用者负责）
         */
        void addUser(const User& user)
        {
            if (usernameIndex.contains(user.GetUsername()))
            {
                throw std::runtime_error("Username '" + user.GetUsername() + "' already exists.");
            }
            users[user.GetId()] = user;
            usernameIndex[user.GetUsername()] = user.GetId();
            if (user.GetId() > _max_user_id)
            {
                _max_user_id = user.GetId();
            }
        }

        /**
         * @brief 删除用户（不检查权限，由调用者负责）
         */
        bool removeUser(int userId)
        {
            auto it = users.find(userId);
            if (it == users.end()) return false;
            usernameIndex.erase(it->second.GetUsername());
            users.erase(it);
            return true;
        }

        /**
         * @brief 更新用户权限（不检查权限，由调用者负责）
         */
        void updateUserPermission(int userId, Permission perm)
        {
            auto it = users.find(userId);
            if (it == users.end())
            {
                throw std::runtime_error("User ID " + std::to_string(userId) + " not found.");
            }
            it->second.SetPermission(perm);
        }

        /**
         * @brief 更新用户密码（不检查权限，由调用者负责）
         */
        void updateUserPassword(int userId, const std::string& newPassword)
        {
            auto it = users.find(userId);
            if (it == users.end())
            {
                throw std::runtime_error("User ID " + std::to_string(userId) + " not found.");
            }
            it->second.SetPasswordHash(simpleHash(newPassword));
        }

        /**
         * @brief 更新用户激活状态（不检查权限，由调用者负责）
         */
        void updateUserActive(int userId, bool active)
        {
            auto it = users.find(userId);
            if (it == users.end())
            {
                throw std::runtime_error("User ID " + std::to_string(userId) + " not found.");
            }
            it->second.SetActive(active);
        }

    private:
        /**
         * @brief 创建默认的 root 用户
         */
        void createDefaultRootUser()
        {
            int rootId = ++_max_user_id;
            User rootUser(rootId, "root", simpleHash("root"), Permission::ROOT);

            users.emplace(rootId, std::move(rootUser));
            usernameIndex["root"] = rootId;

            std::cout << "[UserManager] Created default root user (username: root, password: root)" << std::endl;
            std::cout << "[UserManager] Please change the default password immediately!" << std::endl;
        }
    };
}

