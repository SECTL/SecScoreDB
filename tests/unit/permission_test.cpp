/**
 * @file permission_test.cpp
 * @brief Permission 模块单元测试
 */
#include <gtest/gtest.h>
#include "Permission.h"

using namespace SSDB;

// ============================================================
// Permission 基础功能测试
// ============================================================

class PermissionTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PermissionTest, DefaultPermissionIsNone)
{
    Permission perm = Permission::NONE;
    EXPECT_EQ(static_cast<std::uint8_t>(perm), 0);
}

TEST_F(PermissionTest, HasPermissionChecksCorrectly)
{
    EXPECT_TRUE(hasPermission(Permission::ROOT, Permission::READ));
    EXPECT_TRUE(hasPermission(Permission::ROOT, Permission::WRITE));
    EXPECT_TRUE(hasPermission(Permission::ROOT, Permission::DELETE));

    EXPECT_TRUE(hasPermission(Permission::READ, Permission::READ));
    EXPECT_FALSE(hasPermission(Permission::READ, Permission::WRITE));
    EXPECT_FALSE(hasPermission(Permission::READ, Permission::DELETE));

    EXPECT_TRUE(hasPermission(Permission::READ_WRITE, Permission::READ));
    EXPECT_TRUE(hasPermission(Permission::READ_WRITE, Permission::WRITE));
    EXPECT_FALSE(hasPermission(Permission::READ_WRITE, Permission::DELETE));
}

TEST_F(PermissionTest, AddPermissionCombinesCorrectly)
{
    Permission perm = Permission::NONE;
    perm = addPermission(perm, Permission::READ);
    EXPECT_TRUE(hasPermission(perm, Permission::READ));
    EXPECT_FALSE(hasPermission(perm, Permission::WRITE));

    perm = addPermission(perm, Permission::WRITE);
    EXPECT_TRUE(hasPermission(perm, Permission::READ));
    EXPECT_TRUE(hasPermission(perm, Permission::WRITE));
    EXPECT_EQ(perm, Permission::READ_WRITE);
}

TEST_F(PermissionTest, RemovePermissionWorksCorrectly)
{
    Permission perm = Permission::ROOT;
    perm = removePermission(perm, Permission::DELETE);
    EXPECT_TRUE(hasPermission(perm, Permission::READ));
    EXPECT_TRUE(hasPermission(perm, Permission::WRITE));
    EXPECT_FALSE(hasPermission(perm, Permission::DELETE));
    EXPECT_EQ(perm, Permission::READ_WRITE);
}

TEST_F(PermissionTest, BitwiseOrOperator)
{
    Permission perm = Permission::READ | Permission::WRITE;
    EXPECT_EQ(perm, Permission::READ_WRITE);

    perm = Permission::READ | Permission::DELETE;
    EXPECT_EQ(perm, Permission::READ_DELETE);
}

TEST_F(PermissionTest, BitwiseAndOperator)
{
    Permission perm = Permission::ROOT & Permission::READ;
    EXPECT_EQ(perm, Permission::READ);

    perm = Permission::READ_WRITE & Permission::READ;
    EXPECT_EQ(perm, Permission::READ);
}

TEST_F(PermissionTest, BitwiseNotOperator)
{
    Permission perm = ~Permission::READ;
    EXPECT_FALSE(hasPermission(perm, Permission::READ));
    EXPECT_TRUE(hasPermission(perm, Permission::WRITE));
    EXPECT_TRUE(hasPermission(perm, Permission::DELETE));
}

TEST_F(PermissionTest, PermissionToStringView)
{
    EXPECT_EQ(permissionToStringView(Permission::NONE), "NONE");
    EXPECT_EQ(permissionToStringView(Permission::READ), "READ");
    EXPECT_EQ(permissionToStringView(Permission::WRITE), "WRITE");
    EXPECT_EQ(permissionToStringView(Permission::DELETE), "DELETE");
    EXPECT_EQ(permissionToStringView(Permission::ROOT), "ROOT");
}

TEST_F(PermissionTest, PermissionToStringCombinations)
{
    EXPECT_EQ(permissionToString(Permission::NONE), "NONE");
    EXPECT_EQ(permissionToString(Permission::ROOT), "ROOT");
    EXPECT_EQ(permissionToString(Permission::READ_WRITE), "READ | WRITE");
}

TEST_F(PermissionTest, ParsePermissionFromString)
{
    EXPECT_EQ(parsePermission("ROOT"), Permission::ROOT);
    EXPECT_EQ(parsePermission("root"), Permission::ROOT);
    EXPECT_EQ(parsePermission("NONE"), Permission::NONE);
    EXPECT_EQ(parsePermission("READ"), Permission::READ);
    EXPECT_EQ(parsePermission("READ | WRITE"), Permission::READ_WRITE);
    EXPECT_EQ(parsePermission("read write delete"), Permission::ROOT);
}

// ============================================================
// Constexpr 测试（编译期验证）
// ============================================================

TEST_F(PermissionTest, ConstexprFunctions)
{
    // 这些在编译期就能计算
    constexpr bool hasRead = hasPermission(Permission::ROOT, Permission::READ);
    EXPECT_TRUE(hasRead);

    constexpr Permission combined = addPermission(Permission::READ, Permission::WRITE);
    EXPECT_EQ(combined, Permission::READ_WRITE);

    constexpr std::string_view rootStr = permissionToStringView(Permission::ROOT);
    EXPECT_EQ(rootStr, "ROOT");
}

