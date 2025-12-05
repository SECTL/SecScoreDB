#pragma once

// Windows 头文件定义了 DELETE 宏，需要取消以避免冲突
#ifdef DELETE
#undef DELETE
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace SSDB
{
    /**
     * @brief 权限枚举（使用位标志实现权限组合）
     *
     * NONE  = 0b000 = 0  无权限
     * READ  = 0b001 = 1  只读权限
     * WRITE = 0b010 = 2  写入权限（包含添加、修改数据）
     * DELETE= 0b100 = 4  删除权限（删除记录）
     * ROOT  = 0b111 = 7  全部权限（包括用户管理）
     */
    enum class Permission : uint8_t
    {
        NONE   = 0,
        READ   = 1 << 0,  // 0b001 = 1
        WRITE  = 1 << 1,  // 0b010 = 2
        DELETE = 1 << 2,  // 0b100 = 4

        // 组合权限
        READ_WRITE = READ | WRITE,           // 0b011 = 3  读写
        READ_DELETE = READ | DELETE,         // 0b101 = 5  读删
        WRITE_DELETE = WRITE | DELETE,       // 0b110 = 6  写删
        ROOT = READ | WRITE | DELETE         // 0b111 = 7  全部权限
    };

    // ============================================================
    // 权限操作辅助函数
    // ============================================================

    /**
     * @brief 检查权限是否包含指定权限
     */
    inline bool hasPermission(Permission userPerm, Permission required)
    {
        return (static_cast<uint8_t>(userPerm) & static_cast<uint8_t>(required)) == static_cast<uint8_t>(required);
    }

    /**
     * @brief 添加权限
     */
    inline Permission addPermission(Permission current, Permission toAdd)
    {
        return static_cast<Permission>(static_cast<uint8_t>(current) | static_cast<uint8_t>(toAdd));
    }

    /**
     * @brief 移除权限
     */
    inline Permission removePermission(Permission current, Permission toRemove)
    {
        return static_cast<Permission>(static_cast<uint8_t>(current) & ~static_cast<uint8_t>(toRemove));
    }

    /**
     * @brief 权限组合（或运算）
     */
    inline Permission operator|(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
    }

    /**
     * @brief 权限交集（与运算）
     */
    inline Permission operator&(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
    }

    /**
     * @brief 权限取反
     */
    inline Permission operator~(Permission perm)
    {
        return static_cast<Permission>(~static_cast<uint8_t>(perm) & 0x07); // 保持在3位范围内
    }

    /**
     * @brief 权限转字符串（用于显示/日志）
     */
    inline std::string permissionToString(Permission perm)
    {
        if (perm == Permission::NONE) return "NONE";
        if (perm == Permission::ROOT) return "ROOT";

        std::vector<std::string> perms;
        if (hasPermission(perm, Permission::READ))   perms.push_back("READ");
        if (hasPermission(perm, Permission::WRITE))  perms.push_back("WRITE");
        if (hasPermission(perm, Permission::DELETE)) perms.push_back("DELETE");

        std::string result;
        for (size_t i = 0; i < perms.size(); ++i)
        {
            if (i > 0) result += " | ";
            result += perms[i];
        }
        return result.empty() ? "NONE" : result;
    }

    /**
     * @brief 从字符串解析权限
     */
    inline Permission parsePermission(const std::string& str)
    {
        if (str == "ROOT" || str == "root") return Permission::ROOT;
        if (str == "NONE" || str == "none") return Permission::NONE;

        Permission result = Permission::NONE;
        if (str.find("READ") != std::string::npos || str.find("read") != std::string::npos)
            result = result | Permission::READ;
        if (str.find("WRITE") != std::string::npos || str.find("write") != std::string::npos)
            result = result | Permission::WRITE;
        if (str.find("DELETE") != std::string::npos || str.find("delete") != std::string::npos)
            result = result | Permission::DELETE;

        return result;
    }
}

