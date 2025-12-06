/**
 * @file UserManager.cpp
 * @brief 用户管理器实现
 */
#include "UserManager.h"

#include <format>
#include <iostream>

namespace SSDB
{
    // ============================================================
    // PermissionDeniedException
    // ============================================================

    PermissionDeniedException::PermissionDeniedException(std::string_view operation, Permission required)
        : std::runtime_error(
            std::format("Permission denied: Operation '{}' requires {} permission.",
                       operation, permissionToString(required)))
    {
    }

    PermissionDeniedException::PermissionDeniedException(const std::string& msg)
        : std::runtime_error(msg)
    {
    }

    // ============================================================
    // 构造与析构
    // ============================================================

    UserManager::UserManager(const std::filesystem::path& path)
        : userDb_(path / "users.bin")
    {
        users_ = userDb_.LoadAll<User>();
        rebuildUsernameIndex();

        // 初始化最大 ID
        for (const auto& [id, _] : users_)
        {
            if (id > maxUserId_) maxUserId_ = id;
        }

        // 如果没有用户，创建默认的 root 用户
        if (users_.empty())
        {
            createDefaultRootUser();
        }
    }

    UserManager::~UserManager()
    {
        try
        {
            commit();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[UserManager Error] Failed to save users: " << e.what() << '\n';
        }
    }

    void UserManager::commit()
    {
        userDb_.SaveAll(users_);
    }

    // ============================================================
    // 私有辅助方法
    // ============================================================

    void UserManager::rebuildUsernameIndex()
    {
        usernameIndex_.clear();
        for (const auto& [id, user] : users_)
        {
            usernameIndex_[user.GetUsername()] = id;
        }
    }

    std::string UserManager::hashPassword(const std::string& password)
    {
        std::hash<std::string> hasher;
        return std::to_string(hasher(password + "SSDB_SALT_2024"));
    }

    void UserManager::createDefaultRootUser()
    {
        int rootId = ++maxUserId_;
        User rootUser(rootId, "root", hashPassword("root"), Permission::ROOT);

        users_.emplace(rootId, std::move(rootUser));
        usernameIndex_["root"] = rootId;

        std::cout << "[UserManager] Created default root user (username: root, password: root)\n";
        std::cout << "[UserManager] Please change the default password immediately!\n";
    }

    // ============================================================
    // 用户认证
    // ============================================================

    bool UserManager::login(const std::string& username, const std::string& password)
    {
        auto it = usernameIndex_.find(username);
        if (it == usernameIndex_.end()) return false;

        const User& user = users_.at(it->second);
        if (!user.IsActive()) return false;

        if (user.GetPasswordHash() == hashPassword(password))
        {
            currentUserId_ = user.GetId();
            return true;
        }
        return false;
    }

    void UserManager::logout() noexcept
    {
        currentUserId_ = -1;
    }

    bool UserManager::isLoggedIn() const noexcept
    {
        return currentUserId_ != -1;
    }

    std::optional<std::reference_wrapper<const User>> UserManager::getCurrentUser() const
    {
        if (currentUserId_ == -1) return std::nullopt;
        auto it = users_.find(currentUserId_);
        if (it == users_.end()) return std::nullopt;
        return std::cref(it->second);
    }

    int UserManager::getCurrentUserId() const noexcept
    {
        return currentUserId_;
    }

    // ============================================================
    // 权限检查
    // ============================================================

    bool UserManager::checkPermission(Permission required) const
    {
        if (currentUserId_ == -1) return false;
        auto it = users_.find(currentUserId_);
        if (it == users_.end()) return false;
        return it->second.hasPermission(required);
    }

    void UserManager::requirePermission(Permission required, std::string_view operation) const
    {
        if (!checkPermission(required))
        {
            throw PermissionDeniedException(operation, required);
        }
    }

    bool UserManager::isCurrentUserRoot() const
    {
        if (currentUserId_ == -1) return false;
        auto it = users_.find(currentUserId_);
        if (it == users_.end()) return false;
        return it->second.isRoot();
    }

    // ============================================================
    // 用户管理（需要 root 权限）
    // ============================================================

    User& UserManager::createUser(const std::string& username, const std::string& password, Permission perm)
    {
        if (!isCurrentUserRoot())
        {
            throw PermissionDeniedException("Only root user can create new users.");
        }

        if (usernameIndex_.contains(username))
        {
            throw std::runtime_error(std::format("Username '{}' already exists.", username));
        }

        int newId = ++maxUserId_;
        User newUser(newId, username, hashPassword(password), perm);

        auto [it, success] = users_.emplace(newId, std::move(newUser));
        usernameIndex_[username] = newId;

        return it->second;
    }

    bool UserManager::deleteUser(int userId)
    {
        if (!isCurrentUserRoot())
        {
            throw PermissionDeniedException("Only root user can delete users.");
        }

        if (userId == currentUserId_)
        {
            throw std::runtime_error("Cannot delete the currently logged-in user.");
        }

        auto it = users_.find(userId);
        if (it == users_.end()) return false;

        usernameIndex_.erase(it->second.GetUsername());
        users_.erase(it);
        return true;
    }

    bool UserManager::deleteUser(const std::string& username)
    {
        auto it = usernameIndex_.find(username);
        if (it == usernameIndex_.end()) return false;
        return deleteUser(it->second);
    }

    void UserManager::setUserPermission(int userId, Permission perm)
    {
        if (!isCurrentUserRoot())
        {
            throw PermissionDeniedException("Only root user can modify permissions.");
        }

        auto it = users_.find(userId);
        if (it == users_.end())
        {
            throw std::runtime_error(std::format("User ID {} not found.", userId));
        }

        it->second.SetPermission(perm);
    }

    void UserManager::changePassword(int userId, const std::string& newPassword, const std::string& oldPassword)
    {
        auto it = users_.find(userId);
        if (it == users_.end())
        {
            throw std::runtime_error(std::format("User ID {} not found.", userId));
        }

        // 修改自己的密码需要验证旧密码
        if (userId == currentUserId_)
        {
            if (it->second.GetPasswordHash() != hashPassword(oldPassword))
            {
                throw std::runtime_error("Old password is incorrect.");
            }
        }
        // 修改其他人的密码需要 root 权限
        else if (!isCurrentUserRoot())
        {
            throw PermissionDeniedException("Only root user can change other users' passwords.");
        }

        it->second.SetPasswordHash(hashPassword(newPassword));
    }

    void UserManager::setUserActive(int userId, bool active)
    {
        if (!isCurrentUserRoot())
        {
            throw PermissionDeniedException("Only root user can enable/disable users.");
        }

        if (userId == currentUserId_ && !active)
        {
            throw std::runtime_error("Cannot disable the currently logged-in user.");
        }

        auto it = users_.find(userId);
        if (it == users_.end())
        {
            throw std::runtime_error(std::format("User ID {} not found.", userId));
        }

        it->second.SetActive(active);
    }

    // ============================================================
    // 用户查询
    // ============================================================

    std::optional<std::reference_wrapper<const User>> UserManager::getUser(int userId) const
    {
        auto it = users_.find(userId);
        if (it == users_.end()) return std::nullopt;
        return std::cref(it->second);
    }

    std::optional<std::reference_wrapper<const User>> UserManager::getUser(const std::string& username) const
    {
        auto it = usernameIndex_.find(username);
        if (it == usernameIndex_.end()) return std::nullopt;
        return getUser(it->second);
    }

    bool UserManager::hasUser(int userId) const
    {
        return users_.contains(userId);
    }

    bool UserManager::hasUser(const std::string& username) const
    {
        return usernameIndex_.contains(username);
    }

    const std::unordered_map<int, User>& UserManager::allUsers() const noexcept
    {
        return users_;
    }

    int UserManager::allocateUserId()
    {
        return ++maxUserId_;
    }

    // ============================================================
    // WebSocket API 支持方法
    // ============================================================

    bool UserManager::verifyPassword(int userId, const std::string& password) const
    {
        auto it = users_.find(userId);
        if (it == users_.end()) return false;
        return it->second.GetPasswordHash() == hashPassword(password);
    }

    std::optional<std::reference_wrapper<const User>> UserManager::findUserByUsername(const std::string& username) const
    {
        auto it = usernameIndex_.find(username);
        if (it == usernameIndex_.end()) return std::nullopt;
        auto userIt = users_.find(it->second);
        if (userIt == users_.end()) return std::nullopt;
        return std::cref(userIt->second);
    }

    std::optional<std::reference_wrapper<const User>> UserManager::findUserById(int userId) const
    {
        auto it = users_.find(userId);
        if (it == users_.end()) return std::nullopt;
        return std::cref(it->second);
    }

    int UserManager::getNextUserId() const noexcept
    {
        return maxUserId_ + 1;
    }

    void UserManager::addUser(const User& user)
    {
        if (usernameIndex_.contains(user.GetUsername()))
        {
            throw std::runtime_error(std::format("Username '{}' already exists.", user.GetUsername()));
        }
        users_[user.GetId()] = user;
        usernameIndex_[user.GetUsername()] = user.GetId();
        if (user.GetId() > maxUserId_)
        {
            maxUserId_ = user.GetId();
        }
    }

    bool UserManager::removeUser(int userId)
    {
        auto it = users_.find(userId);
        if (it == users_.end()) return false;
        usernameIndex_.erase(it->second.GetUsername());
        users_.erase(it);
        return true;
    }

    void UserManager::updateUserPermission(int userId, Permission perm)
    {
        auto it = users_.find(userId);
        if (it == users_.end())
        {
            throw std::runtime_error(std::format("User ID {} not found.", userId));
        }
        it->second.SetPermission(perm);
    }

    void UserManager::updateUserPassword(int userId, const std::string& newPassword)
    {
        auto it = users_.find(userId);
        if (it == users_.end())
        {
            throw std::runtime_error(std::format("User ID {} not found.", userId));
        }
        it->second.SetPasswordHash(hashPassword(newPassword));
    }

    void UserManager::updateUserActive(int userId, bool active)
    {
        auto it = users_.find(userId);
        if (it == users_.end())
        {
            throw std::runtime_error(std::format("User ID {} not found.", userId));
        }
        it->second.SetActive(active);
    }

} // namespace SSDB

