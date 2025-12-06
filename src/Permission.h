/**
 * @file Permission.h
 * @brief 权限系统定义
 */
#pragma once

// Windows 头文件定义了 DELETE 宏，需要取消以避免冲突
#ifdef DELETE
#undef DELETE
#endif

#include <cstdint>
#include <string>
#include <string_view>

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
    enum class Permission : std::uint8_t
    {
        NONE   = 0,
        READ   = 1U << 0,  // 0b001 = 1
        WRITE  = 1U << 1,  // 0b010 = 2
        DELETE = 1U << 2,  // 0b100 = 4

        // 组合权限
        READ_WRITE   = READ | WRITE,           // 0b011 = 3
        READ_DELETE  = READ | DELETE,          // 0b101 = 5
        WRITE_DELETE = WRITE | DELETE,         // 0b110 = 6
        ROOT         = READ | WRITE | DELETE   // 0b111 = 7
    };

    // ============================================================
    // 权限操作辅助函数（constexpr + [[nodiscard]]）
    // ============================================================

    /**
     * @brief 检查权限是否包含指定权限
     */
    [[nodiscard]] constexpr bool hasPermission(Permission userPerm, Permission required) noexcept
    {
        const auto user = static_cast<std::uint8_t>(userPerm);
        const auto req  = static_cast<std::uint8_t>(required);
        return (user & req) == req;
    }

    /**
     * @brief 添加权限
     */
    [[nodiscard]] constexpr Permission addPermission(Permission current, Permission toAdd) noexcept
    {
        return static_cast<Permission>(
            static_cast<std::uint8_t>(current) | static_cast<std::uint8_t>(toAdd)
        );
    }

    /**
     * @brief 移除权限
     */
    [[nodiscard]] constexpr Permission removePermission(Permission current, Permission toRemove) noexcept
    {
        return static_cast<Permission>(
            static_cast<std::uint8_t>(current) & ~static_cast<std::uint8_t>(toRemove)
        );
    }

    /**
     * @brief 权限组合（或运算）
     */
    [[nodiscard]] constexpr Permission operator|(Permission lhs, Permission rhs) noexcept
    {
        return static_cast<Permission>(
            static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs)
        );
    }

    /**
     * @brief 权限交集（与运算）
     */
    [[nodiscard]] constexpr Permission operator&(Permission lhs, Permission rhs) noexcept
    {
        return static_cast<Permission>(
            static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs)
        );
    }

    /**
     * @brief 权限取反
     */
    [[nodiscard]] constexpr Permission operator~(Permission perm) noexcept
    {
        return static_cast<Permission>(
            ~static_cast<std::uint8_t>(perm) & 0x07U  // 保持在3位范围内
        );
    }

    /**
     * @brief 权限转字符串视图（编译期，用于简单情况）
     */
    [[nodiscard]] constexpr std::string_view permissionToStringView(Permission perm) noexcept
    {
        switch (perm)
        {
            case Permission::NONE:         return "NONE";
            case Permission::READ:         return "READ";
            case Permission::WRITE:        return "WRITE";
            case Permission::DELETE:       return "DELETE";
            case Permission::READ_WRITE:   return "READ_WRITE";
            case Permission::READ_DELETE:  return "READ_DELETE";
            case Permission::WRITE_DELETE: return "WRITE_DELETE";
            case Permission::ROOT:         return "ROOT";
            default:                       return "UNKNOWN";
        }
    }

    /**
     * @brief 权限转字符串（用于显示/日志，支持组合权限详细展示）
     */
    [[nodiscard]] inline std::string permissionToString(Permission perm)
    {
        if (perm == Permission::NONE) return "NONE";
        if (perm == Permission::ROOT) return "ROOT";

        std::string result;
        if (hasPermission(perm, Permission::READ))
        {
            result += "READ";
        }
        if (hasPermission(perm, Permission::WRITE))
        {
            if (!result.empty()) result += " | ";
            result += "WRITE";
        }
        if (hasPermission(perm, Permission::DELETE))
        {
            if (!result.empty()) result += " | ";
            result += "DELETE";
        }
        return result.empty() ? "NONE" : result;
    }

    /**
     * @brief 从字符串解析权限
     */
    [[nodiscard]] inline Permission parsePermission(std::string_view str) noexcept
    {
        if (str == "ROOT" || str == "root") return Permission::ROOT;
        if (str == "NONE" || str == "none") return Permission::NONE;

        Permission result = Permission::NONE;
        if (str.find("READ") != std::string_view::npos || str.find("read") != std::string_view::npos)
        {
            result = result | Permission::READ;
        }
        if (str.find("WRITE") != std::string_view::npos || str.find("write") != std::string_view::npos)
        {
            result = result | Permission::WRITE;
        }
        if (str.find("DELETE") != std::string_view::npos || str.find("delete") != std::string_view::npos)
        {
            result = result | Permission::DELETE;
        }

        return result;
    }
}

