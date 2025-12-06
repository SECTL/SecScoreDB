#include "SecScoreDB.h"
#include "Permission.h"
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <string_view>

using namespace SSDB;

static void cleanup(const std::filesystem::path& p)
{
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

static void waitForUser(const std::string& message = "Press Enter to continue...")
{
    std::cout << "\n[PAUSE] " << message << std::endl;
    std::cin.get();
}

// 计时辅助类
class Timer
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint start_;
    std::string name_;

public:
    explicit Timer(std::string name) : start_(Clock::now()), name_(std::move(name)) {}

    ~Timer()
    {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        std::cout << "[TIMER] " << name_ << ": " << duration << " ms" << std::endl;
    }

    // 手动停止计时并返回毫秒数
    long long stop()
    {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        std::cout << "[TIMER] " << name_ << ": " << duration << " ms" << std::endl;
        return duration;
    }
};

namespace
{
    constexpr int kStudentCount = 500;
    constexpr int kGroupCount = 30;
    constexpr int kEventCount = 200'000;
    constexpr int kStudentBaseId = 10'000;
    constexpr int kGroupBaseId = 20'000;

    SchemaDef makeStudentSchema()
    {
        return {
            {"name", FieldType::String},
            {"age", FieldType::Int},
            {"score", FieldType::Double}
        };
    }

    SchemaDef makeGroupSchema()
    {
        return {
            {"title", FieldType::String},
            {"level", FieldType::Int}
        };
    }

    void logPhase(std::string_view message)
    {
        std::cout << "[LOG] " << message << std::endl;
    }

    void logProgress(std::string_view phase, int current, int total, int step)
    {
        if (current % step == 0 || current == total)
        {
            std::cout << "[LOG] " << phase << " " << current << "/" << total << std::endl;
        }
    }

    int studentIndexFromId(int id)
    {
        return id - kStudentBaseId;
    }

    int groupIndexFromId(int id)
    {
        return id - kGroupBaseId;
    }

    std::string studentNameFor(int id)
    {
        return "Student-" + std::to_string(id);
    }

    int studentAgeFor(int id)
    {
        return 18 + (studentIndexFromId(id) % 10);
    }

    double studentScoreFor(int id)
    {
        return 60.0 + static_cast<double>(studentIndexFromId(id) % 40);
    }

    std::string groupTitleFor(int id)
    {
        return "Group-" + std::to_string(id);
    }

    int groupLevelFor(int id)
    {
        return 1 + (groupIndexFromId(id) % 5);
    }

    void seedGroups(SecScoreDB& db)
    {
        Timer timer("Seeding groups");
        logPhase("Seeding groups...");
        for (int i = 0; i < kGroupCount; ++i)
        {
            int id = kGroupBaseId + i;
            auto group = db.createGroup(id);
            group["title"] = std::string(groupTitleFor(id));
            group["level"] = groupLevelFor(id);
            logProgress("Groups created", i + 1, kGroupCount, 5);
        }
    }

    void seedStudents(SecScoreDB& db)
    {
        Timer timer("Seeding students");
        logPhase("Seeding students...");
        for (int i = 0; i < kStudentCount; ++i)
        {
            int id = kStudentBaseId + i;
            auto student = db.createStudent(id);
            student["name"] = std::string(studentNameFor(id));
            student["age"] = studentAgeFor(id);
            student["score"] = studentScoreFor(id);
            logProgress("Students created", i + 1, kStudentCount, 50);
        }
    }

    struct EventStats
    {
        size_t total = 0;
        size_t studentEvents = 0;
        size_t groupEvents = 0;
        size_t erasedEvents = 0;
    };

    EventStats emitEvents(SecScoreDB& db)
    {
        Timer timer("Generating events");
        logPhase("Generating events...");
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> studentOffset(0, kStudentCount - 1);
        std::uniform_int_distribution<int> groupOffset(0, kGroupCount - 1);
        std::uniform_int_distribution<int> operatorDist(900, 1100);
        std::uniform_int_distribution<int> deltaDist(-20, 20);
        std::uniform_int_distribution<int> eventTypeRoll(0, 99);

        EventStats stats{};
        stats.total = kEventCount;

        for (int i = 1; i <= kEventCount; ++i)
        {
            bool studentEvent = eventTypeRoll(rng) < 70;
            int targetId = studentEvent
                               ? (kStudentBaseId + studentOffset(rng))
                               : (kGroupBaseId + groupOffset(rng));
            std::string reason = studentEvent
                                     ? "Student adjustment #" + std::to_string(i)
                                     : "Group adjustment #" + std::to_string(i);
            Event evt(INVALID_ID,
                      studentEvent ? EventType::Student : EventType::Group,
                      targetId,
                      std::move(reason),
                      operatorDist(rng),
                      deltaDist(rng));
            int assignedId = db.addEvent(std::move(evt));

            if (studentEvent)
            {
                ++stats.studentEvents;
            }
            else
            {
                ++stats.groupEvents;
            }

            if (i % 10 == 0)
            {
                db.setEventErased(assignedId, true);
                ++stats.erasedEvents;
            }

            if (i % 20'000 == 0)
            {
                std::cout << "[LOG] Events generated " << i << "/" << kEventCount << std::endl;
            }
        }

        logPhase("Events generation complete.");
        return stats;
    }
}

int main()
{
    std::cout << "[TEST] SecScoreDB stress test start" << std::endl;
    const std::filesystem::path dbPath{"./testdata"};
    cleanup(dbPath);

    SchemaDef stuSchema = makeStudentSchema();
    SchemaDef grpSchema = makeGroupSchema();

    EventStats emittedStats{};

    {
        Timer phaseTimer("Phase 1 - Initial data creation");
        SecScoreDB db(dbPath);
        db.initStudentSchema(stuSchema);
        db.initGroupSchema(grpSchema);

        seedGroups(db);
        seedStudents(db);
        emittedStats = emitEvents(db);

        {
            Timer commitTimer("Commit to disk");
            db.commit();
        }
        logPhase("Initial commit complete.");
        waitForUser("Phase 1 complete: Initial data created. Check memory usage now.");
    }

    {
        Timer phaseTimer("Phase 2 - Data verification");
        logPhase("Re-opening database for verification...");

        Timer loadTimer("Load database from disk");
        SecScoreDB db(dbPath);
        loadTimer.stop();

        db.initStudentSchema(stuSchema);
        db.initGroupSchema(grpSchema);

        std::array<int, 3> sampleStudents{
            kStudentBaseId,
            kStudentBaseId + kStudentCount / 2,
            kStudentBaseId + kStudentCount - 1
        };

        for (int sid : sampleStudents)
        {
            auto student = db.getStudent(sid);
            assert(static_cast<std::string>(student["name"]) == studentNameFor(sid));
            assert(static_cast<int>(student["age"]) == studentAgeFor(sid));
            assert(static_cast<double>(student["score"]) == studentScoreFor(sid));
            std::cout << "[VERIFY] Student " << sid << " verified." << std::endl;
        }

        std::array<int, 2> sampleGroups{
            kGroupBaseId,
            kGroupBaseId + kGroupCount - 1
        };

        for (int gid : sampleGroups)
        {
            auto group = db.getGroup(gid);
            assert(static_cast<std::string>(group["title"]) == groupTitleFor(gid));
            assert(static_cast<int>(group["level"]) == groupLevelFor(gid));
            std::cout << "[VERIFY] Group " << gid << " verified." << std::endl;
        }

        size_t totalCount, erasedCount;
        {
            Timer queryTimer("Query all events");
            auto totalEvents = db.getEvents([](const Event&) { return true; });
            totalCount = totalEvents.size();
        }
        {
            Timer queryTimer("Query erased events");
            auto erasedEvents = db.getEvents([](const Event& e) { return e.IsErased(); });
            erasedCount = erasedEvents.size();
        }

        std::cout << "[VERIFY] Total events stored: " << totalCount << std::endl;
        std::cout << "[VERIFY] Erased events stored: " << erasedCount << std::endl;
        std::cout << "[VERIFY] Expected totals: " << emittedStats.total
            << ", expected erased: " << emittedStats.erasedEvents << std::endl;

        assert(totalCount == emittedStats.total);
        assert(erasedCount == emittedStats.erasedEvents);

        std::cout << "[TEST] Verification completed successfully." << std::endl;
        waitForUser("Phase 2 complete: Data verification done. Check memory usage now.");
    }

    // ============================================================
    // 用户权限系统测试
    // ============================================================
    const std::filesystem::path authDbPath{"./testdata_auth"};
    cleanup(authDbPath);

    {
        logPhase("Testing user authentication and permissions...");

        SecScoreDB db(authDbPath);
        db.initStudentSchema(stuSchema);
        db.initGroupSchema(grpSchema);

        // 测试1: 未登录状态
        logPhase("Test 1: Check not logged in initially");
        assert(!db.isLoggedIn());
        assert(!db.checkPermission(Permission::READ));
        std::cout << "[PASS] Not logged in initially" << std::endl;

        // 测试2: 使用默认 root 账户登录
        logPhase("Test 2: Login with default root account");
        bool loginSuccess = db.login("root", "root");
        assert(loginSuccess);
        assert(db.isLoggedIn());
        assert(db.checkPermission(Permission::ROOT));
        assert(db.checkPermission(Permission::READ));
        assert(db.checkPermission(Permission::WRITE));
        assert(db.checkPermission(Permission::DELETE));
        std::cout << "[PASS] Root login successful with all permissions" << std::endl;

        // 测试3: 创建用户（需要 root 权限）
        logPhase("Test 3: Create users with different permissions");
        auto& userMgr = db.userManager();

        // 创建只读用户
        User& readOnlyUser = userMgr.createUser("reader", "reader123", Permission::READ);
        assert(readOnlyUser.GetUsername() == "reader");
        assert(readOnlyUser.canRead());
        assert(!readOnlyUser.canWrite());
        assert(!readOnlyUser.canDelete());
        std::cout << "[PASS] Created read-only user" << std::endl;

        // 创建读写用户
        User& readWriteUser = userMgr.createUser("editor", "editor123", Permission::READ | Permission::WRITE);
        assert(readWriteUser.canRead());
        assert(readWriteUser.canWrite());
        assert(!readWriteUser.canDelete());
        std::cout << "[PASS] Created read-write user" << std::endl;

        // 创建全权限用户
        User& adminUser = userMgr.createUser("admin", "admin123", Permission::ROOT);
        assert(adminUser.isRoot());
        std::cout << "[PASS] Created admin user with ROOT permission" << std::endl;

        // 测试4: 登出
        logPhase("Test 4: Logout");
        db.logout();
        assert(!db.isLoggedIn());
        std::cout << "[PASS] Logout successful" << std::endl;

        // 测试5: 错误密码登录
        logPhase("Test 5: Login with wrong password");
        bool wrongLogin = db.login("reader", "wrongpassword");
        assert(!wrongLogin);
        assert(!db.isLoggedIn());
        std::cout << "[PASS] Wrong password rejected" << std::endl;
        waitForUser("Phase 3a complete: Basic auth tests (1-5) done.");

        // 测试6: 只读用户登录并检查权限
        logPhase("Test 6: Read-only user permissions");
        assert(db.login("reader", "reader123"));
        assert(db.isLoggedIn());
        assert(db.checkPermission(Permission::READ));
        assert(!db.checkPermission(Permission::WRITE));
        assert(!db.checkPermission(Permission::DELETE));
        std::cout << "[PASS] Read-only user has correct permissions" << std::endl;

        // 测试7: 只读用户尝试创建用户（应该失败）
        logPhase("Test 7: Read-only user cannot create users");
        bool exceptionThrown = false;
        try {
            userMgr.createUser("hacker", "hack123", Permission::ROOT);
        } catch (const PermissionDeniedException& e) {
            exceptionThrown = true;
            std::cout << "[EXPECTED] " << e.what() << std::endl;
        }
        assert(exceptionThrown);
        std::cout << "[PASS] Read-only user cannot create users" << std::endl;

        // 测试8: 读写用户登录
        logPhase("Test 8: Read-write user permissions");
        db.logout();
        assert(db.login("editor", "editor123"));
        assert(db.checkPermission(Permission::READ));
        assert(db.checkPermission(Permission::WRITE));
        assert(!db.checkPermission(Permission::DELETE));
        std::cout << "[PASS] Read-write user has correct permissions" << std::endl;

        // 测试9: requirePermission 测试
        logPhase("Test 9: requirePermission throws on missing permission");
        exceptionThrown = false;
        try {
            db.requirePermission(Permission::DELETE, "delete student");
        } catch (const PermissionDeniedException& e) {
            exceptionThrown = true;
            std::cout << "[EXPECTED] " << e.what() << std::endl;
        }
        assert(exceptionThrown);
        std::cout << "[PASS] requirePermission correctly throws exception" << std::endl;

        // 测试10: 权限组合测试
        logPhase("Test 10: Permission combination");
        Permission combined = Permission::READ | Permission::DELETE;
        assert(hasPermission(combined, Permission::READ));
        assert(!hasPermission(combined, Permission::WRITE));
        assert(hasPermission(combined, Permission::DELETE));
        std::cout << "[PASS] Permission combination works correctly" << std::endl;
        waitForUser("Phase 3b complete: Permission tests (6-10) done.");

        // 测试11: 修改用户权限（需要 root）
        logPhase("Test 11: Modify user permission (requires root)");
        db.logout();
        assert(db.login("admin", "admin123"));
        int editorId = readWriteUser.GetId();
        userMgr.setUserPermission(editorId, Permission::ROOT);
        auto editorOpt = userMgr.getUser(editorId);
        assert(editorOpt.has_value());
        assert(editorOpt->get().isRoot());
        std::cout << "[PASS] User permission modified successfully" << std::endl;

        // 测试12: 删除用户
        logPhase("Test 12: Delete user");
        int readerId = readOnlyUser.GetId();
        bool deleted = userMgr.deleteUser(readerId);
        assert(deleted);
        assert(!userMgr.hasUser(readerId));
        assert(!userMgr.hasUser("reader"));
        std::cout << "[PASS] User deleted successfully" << std::endl;

        // 测试13: 修改密码
        logPhase("Test 13: Change password");
        db.logout();
        assert(db.login("editor", "editor123"));
        userMgr.changePassword(editorId, "newpassword", "editor123");
        db.logout();
        assert(!db.login("editor", "editor123"));  // 旧密码失败
        assert(db.login("editor", "newpassword"));  // 新密码成功
        std::cout << "[PASS] Password changed successfully" << std::endl;

        // 测试14: 禁用用户
        logPhase("Test 14: Disable user");
        db.logout();
        assert(db.login("admin", "admin123"));
        userMgr.setUserActive(editorId, false);
        db.logout();
        assert(!db.login("editor", "newpassword"));  // 禁用后无法登录
        std::cout << "[PASS] Disabled user cannot login" << std::endl;
        waitForUser("Phase 3c complete: User management tests (11-14) done.");

        // 测试15: 持久化测试
        logPhase("Test 15: User persistence");
        db.commit();
    }  // db 在这里析构，确保文件完全写入

    waitForUser("Phase 3d: Database closed. About to test persistence...");

    // 重新打开数据库验证持久化
    {
        SecScoreDB db2(authDbPath);
        assert(db2.login("admin", "admin123"));
        assert(db2.userManager().hasUser("admin"));
        assert(!db2.userManager().hasUser("reader"));  // 已删除
        std::cout << "[PASS] User data persisted correctly" << std::endl;

        logPhase("All user authentication tests passed!");
    }

    std::cout << "[TEST] SecScoreDB stress test finished" << std::endl;
    waitForUser("All tests complete! Check final memory usage.");

    return 0;
}
