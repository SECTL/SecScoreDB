/**
 * @file auth_test.cpp
 * @brief 用户认证和权限系统集成测试
 */
#include <gtest/gtest.h>
#include "SecScoreDB.h"
#include <filesystem>

using namespace SSDB;

// ============================================================
// 认证系统测试
// ============================================================

class AuthTest : public ::testing::Test
{
protected:
    std::filesystem::path testDbPath{"./test_auth_gtest"};
    SchemaDef studentSchema;
    SchemaDef groupSchema;

    void SetUp() override
    {
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);

        studentSchema = {
            {"name", FieldType::String},
            {"age", FieldType::Int}
        };

        groupSchema = {
            {"title", FieldType::String}
        };
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
    }
};

TEST_F(AuthTest, NotLoggedInInitially)
{
    SecScoreDB db(testDbPath);
    EXPECT_FALSE(db.isLoggedIn());
}

TEST_F(AuthTest, NotLoggedInHasNoPermissions)
{
    SecScoreDB db(testDbPath);
    EXPECT_FALSE(db.checkPermission(Permission::READ));
    EXPECT_FALSE(db.checkPermission(Permission::WRITE));
    EXPECT_FALSE(db.checkPermission(Permission::DELETE));
}

TEST_F(AuthTest, LoginWithDefaultRoot)
{
    SecScoreDB db(testDbPath);

    bool success = db.login("root", "root");
    EXPECT_TRUE(success);
    EXPECT_TRUE(db.isLoggedIn());
}

TEST_F(AuthTest, RootHasAllPermissions)
{
    SecScoreDB db(testDbPath);
    db.login("root", "root");

    EXPECT_TRUE(db.checkPermission(Permission::READ));
    EXPECT_TRUE(db.checkPermission(Permission::WRITE));
    EXPECT_TRUE(db.checkPermission(Permission::DELETE));
    EXPECT_TRUE(db.checkPermission(Permission::ROOT));
}

TEST_F(AuthTest, LoginWithWrongPassword)
{
    SecScoreDB db(testDbPath);

    bool success = db.login("root", "wrongpassword");
    EXPECT_FALSE(success);
    EXPECT_FALSE(db.isLoggedIn());
}

TEST_F(AuthTest, LoginWithNonexistentUser)
{
    SecScoreDB db(testDbPath);

    bool success = db.login("nonexistent", "password");
    EXPECT_FALSE(success);
    EXPECT_FALSE(db.isLoggedIn());
}

TEST_F(AuthTest, Logout)
{
    SecScoreDB db(testDbPath);

    db.login("root", "root");
    EXPECT_TRUE(db.isLoggedIn());

    db.logout();
    EXPECT_FALSE(db.isLoggedIn());
}

TEST_F(AuthTest, CreateUserAsRoot)
{
    SecScoreDB db(testDbPath);
    db.login("root", "root");

    auto& userMgr = db.userManager();
    User& newUser = userMgr.createUser("reader", "pass123", Permission::READ);

    EXPECT_EQ(newUser.GetUsername(), "reader");
    EXPECT_TRUE(newUser.canRead());
    EXPECT_FALSE(newUser.canWrite());
}

TEST_F(AuthTest, CreateUserWithDifferentPermissions)
{
    SecScoreDB db(testDbPath);
    db.login("root", "root");

    auto& userMgr = db.userManager();

    User& readOnly = userMgr.createUser("readonly", "pass", Permission::READ);
    EXPECT_TRUE(readOnly.canRead());
    EXPECT_FALSE(readOnly.canWrite());
    EXPECT_FALSE(readOnly.canDelete());

    User& readWrite = userMgr.createUser("readwrite", "pass", Permission::READ_WRITE);
    EXPECT_TRUE(readWrite.canRead());
    EXPECT_TRUE(readWrite.canWrite());
    EXPECT_FALSE(readWrite.canDelete());

    User& admin = userMgr.createUser("admin", "pass", Permission::ROOT);
    EXPECT_TRUE(admin.isRoot());
}

TEST_F(AuthTest, NonRootCannotCreateUsers)
{
    SecScoreDB db(testDbPath);

    // 先用 root 创建一个普通用户
    db.login("root", "root");
    db.userManager().createUser("normaluser", "pass", Permission::READ);
    db.logout();

    // 用普通用户登录
    db.login("normaluser", "pass");
    EXPECT_TRUE(db.isLoggedIn());

    // 尝试创建用户应该失败
    EXPECT_THROW(
        db.userManager().createUser("hacker", "hack", Permission::ROOT),
        PermissionDeniedException
    );
}

TEST_F(AuthTest, LoginAsCreatedUser)
{
    SecScoreDB db(testDbPath);

    // 创建用户
    db.login("root", "root");
    db.userManager().createUser("testuser", "testpass", Permission::READ_WRITE);
    db.logout();

    // 用新用户登录
    bool success = db.login("testuser", "testpass");
    EXPECT_TRUE(success);
    EXPECT_TRUE(db.checkPermission(Permission::READ));
    EXPECT_TRUE(db.checkPermission(Permission::WRITE));
    EXPECT_FALSE(db.checkPermission(Permission::DELETE));
}

TEST_F(AuthTest, RequirePermissionThrowsOnMissing)
{
    SecScoreDB db(testDbPath);

    db.login("root", "root");
    db.userManager().createUser("reader", "pass", Permission::READ);
    db.logout();

    db.login("reader", "pass");

    // READ 权限应该通过
    EXPECT_NO_THROW(db.requirePermission(Permission::READ, "read data"));

    // WRITE 权限应该抛出异常
    EXPECT_THROW(
        db.requirePermission(Permission::WRITE, "write data"),
        PermissionDeniedException
    );
}

TEST_F(AuthTest, DeleteUser)
{
    SecScoreDB db(testDbPath);
    db.login("root", "root");

    auto& userMgr = db.userManager();
    User& user = userMgr.createUser("tobedeleted", "pass", Permission::READ);
    int userId = user.GetId();

    EXPECT_TRUE(userMgr.hasUser(userId));
    EXPECT_TRUE(userMgr.hasUser("tobedeleted"));

    bool deleted = userMgr.deleteUser(userId);
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(userMgr.hasUser(userId));
    EXPECT_FALSE(userMgr.hasUser("tobedeleted"));
}

TEST_F(AuthTest, ChangePassword)
{
    SecScoreDB db(testDbPath);
    db.login("root", "root");

    auto& userMgr = db.userManager();
    User& user = userMgr.createUser("pwtest", "oldpass", Permission::READ);
    int userId = user.GetId();

    db.logout();

    // 用旧密码登录
    EXPECT_TRUE(db.login("pwtest", "oldpass"));

    // 修改密码
    userMgr.changePassword(userId, "newpass", "oldpass");
    db.logout();

    // 旧密码应该失败
    EXPECT_FALSE(db.login("pwtest", "oldpass"));

    // 新密码应该成功
    EXPECT_TRUE(db.login("pwtest", "newpass"));
}

TEST_F(AuthTest, DisableUser)
{
    SecScoreDB db(testDbPath);
    db.login("root", "root");

    auto& userMgr = db.userManager();
    User& user = userMgr.createUser("tobedeactivated", "pass", Permission::READ);
    int userId = user.GetId();

    // 禁用用户
    userMgr.setUserActive(userId, false);
    db.logout();

    // 禁用后无法登录
    EXPECT_FALSE(db.login("tobedeactivated", "pass"));
}

TEST_F(AuthTest, ModifyUserPermission)
{
    SecScoreDB db(testDbPath);
    db.login("root", "root");

    auto& userMgr = db.userManager();
    User& user = userMgr.createUser("permtest", "pass", Permission::READ);
    int userId = user.GetId();

    // 验证初始权限
    EXPECT_TRUE(user.canRead());
    EXPECT_FALSE(user.canWrite());

    // 修改权限
    userMgr.setUserPermission(userId, Permission::ROOT);

    // 获取用户并验证
    auto userOpt = userMgr.getUser(userId);
    EXPECT_TRUE(userOpt.has_value());
    EXPECT_TRUE(userOpt->get().isRoot());
}

TEST_F(AuthTest, UserPersistence)
{
    // 创建用户并保存
    {
        SecScoreDB db(testDbPath);
        db.login("root", "root");
        db.userManager().createUser("persistent", "pass123", Permission::READ_WRITE);
        db.commit();
    }

    // 重新加载并验证
    {
        SecScoreDB db(testDbPath);
        EXPECT_TRUE(db.login("persistent", "pass123"));
        EXPECT_TRUE(db.checkPermission(Permission::READ));
        EXPECT_TRUE(db.checkPermission(Permission::WRITE));
    }
}

