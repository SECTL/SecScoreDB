/**
 * @file user_test.cpp
 * @brief User 实体单元测试
 */
#include <gtest/gtest.h>
#include "User.h"

using namespace SSDB;

// ============================================================
// User 测试
// ============================================================

class UserTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(UserTest, DefaultConstruction)
{
    User u;
    EXPECT_EQ(u.GetId(), 0);
    EXPECT_TRUE(u.GetUsername().empty());
    EXPECT_TRUE(u.GetPasswordHash().empty());
    EXPECT_EQ(u.GetPermission(), Permission::NONE);
    EXPECT_TRUE(u.IsActive());
}

TEST_F(UserTest, ParameterizedConstruction)
{
    User u(1, "admin", "hash123", Permission::ROOT);

    EXPECT_EQ(u.GetId(), 1);
    EXPECT_EQ(u.GetUsername(), "admin");
    EXPECT_EQ(u.GetPasswordHash(), "hash123");
    EXPECT_EQ(u.GetPermission(), Permission::ROOT);
    EXPECT_TRUE(u.IsActive());
}

TEST_F(UserTest, DefaultPermissionIsRead)
{
    User u(1, "reader", "hash");
    EXPECT_EQ(u.GetPermission(), Permission::READ);
}

TEST_F(UserTest, SettersAndGetters)
{
    User u;

    u.SetId(42);
    EXPECT_EQ(u.GetId(), 42);

    u.SetUsername("testuser");
    EXPECT_EQ(u.GetUsername(), "testuser");

    u.SetPasswordHash("newhash");
    EXPECT_EQ(u.GetPasswordHash(), "newhash");

    u.SetPermission(Permission::READ_WRITE);
    EXPECT_EQ(u.GetPermission(), Permission::READ_WRITE);

    u.SetActive(false);
    EXPECT_FALSE(u.IsActive());
}

TEST_F(UserTest, PermissionChecks)
{
    User u(1, "user", "hash", Permission::READ_WRITE);

    EXPECT_TRUE(u.hasPermission(Permission::READ));
    EXPECT_TRUE(u.hasPermission(Permission::WRITE));
    EXPECT_FALSE(u.hasPermission(Permission::DELETE));
    EXPECT_FALSE(u.isRoot());
    EXPECT_TRUE(u.canRead());
    EXPECT_TRUE(u.canWrite());
    EXPECT_FALSE(u.canDelete());
}

TEST_F(UserTest, RootUserHasAllPermissions)
{
    User u(1, "root", "hash", Permission::ROOT);

    EXPECT_TRUE(u.isRoot());
    EXPECT_TRUE(u.canRead());
    EXPECT_TRUE(u.canWrite());
    EXPECT_TRUE(u.canDelete());
    EXPECT_TRUE(u.hasPermission(Permission::ROOT));
}

TEST_F(UserTest, AddPermission)
{
    User u(1, "user", "hash", Permission::READ);

    EXPECT_TRUE(u.canRead());
    EXPECT_FALSE(u.canWrite());

    u.addPermission(Permission::WRITE);
    EXPECT_TRUE(u.canRead());
    EXPECT_TRUE(u.canWrite());
    EXPECT_EQ(u.GetPermission(), Permission::READ_WRITE);
}

TEST_F(UserTest, RemovePermission)
{
    User u(1, "user", "hash", Permission::ROOT);

    u.removePermission(Permission::DELETE);
    EXPECT_TRUE(u.canRead());
    EXPECT_TRUE(u.canWrite());
    EXPECT_FALSE(u.canDelete());
    EXPECT_FALSE(u.isRoot());
}

TEST_F(UserTest, MoveSemantics)
{
    User u1(1, "user1", "hash1", Permission::READ);

    // 测试移动构造
    User u2 = std::move(u1);
    EXPECT_EQ(u2.GetId(), 1);
    EXPECT_EQ(u2.GetUsername(), "user1");
}

