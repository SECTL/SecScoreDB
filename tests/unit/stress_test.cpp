/**
 * @file stress_test.cpp
 * @brief 大数据压力测试（带计时和内存计量）
 */
#include <gtest/gtest.h>
#include "SecScoreDB.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

using namespace SSDB;

// ============================================================
// 计时和内存辅助工具
// ============================================================

namespace TestUtils
{
    /**
     * @brief 高精度计时器
     */
    class Timer
    {
    public:
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = Clock::time_point;
        using Duration = std::chrono::duration<double, std::milli>;

    private:
        std::string name_;
        TimePoint start_;
        bool stopped_ = false;
        double elapsed_ = 0.0;

    public:
        explicit Timer(std::string name)
            : name_(std::move(name))
            , start_(Clock::now())
        {
        }

        ~Timer()
        {
            if (!stopped_)
            {
                stop();
            }
        }

        double stop()
        {
            if (!stopped_)
            {
                auto end = Clock::now();
                elapsed_ = Duration(end - start_).count();
                stopped_ = true;

                std::cout << "[TIMER] " << name_ << ": "
                          << std::fixed << std::setprecision(2)
                          << elapsed_ << " ms" << std::endl;
            }
            return elapsed_;
        }

        [[nodiscard]] double elapsed() const
        {
            if (stopped_)
            {
                return elapsed_;
            }
            return Duration(Clock::now() - start_).count();
        }

        // 禁用拷贝
        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;
    };

    /**
     * @brief 内存使用量测量（跨平台）
     */
    struct MemoryInfo
    {
        std::size_t currentBytes = 0;   // 当前使用内存（字节）
        std::size_t peakBytes = 0;      // 峰值内存（字节）

        [[nodiscard]] double currentMB() const
        {
            return static_cast<double>(currentBytes) / (1024.0 * 1024.0);
        }

        [[nodiscard]] double peakMB() const
        {
            return static_cast<double>(peakBytes) / (1024.0 * 1024.0);
        }
    };

    /**
     * @brief 获取当前进程内存使用量
     */
    [[nodiscard]] inline MemoryInfo getMemoryUsage()
    {
        MemoryInfo info;

#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),
                                  reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                  sizeof(pmc)))
        {
            info.currentBytes = pmc.WorkingSetSize;
            info.peakBytes = pmc.PeakWorkingSetSize;
        }
#else
        // Linux/Unix: 读取 /proc/self/status
        std::ifstream statusFile("/proc/self/status");
        std::string line;
        while (std::getline(statusFile, line))
        {
            if (line.substr(0, 6) == "VmRSS:")
            {
                std::size_t kb = 0;
                std::sscanf(line.c_str(), "VmRSS: %zu kB", &kb);
                info.currentBytes = kb * 1024;
            }
            else if (line.substr(0, 6) == "VmHWM:")
            {
                std::size_t kb = 0;
                std::sscanf(line.c_str(), "VmHWM: %zu kB", &kb);
                info.peakBytes = kb * 1024;
            }
        }
#endif
        return info;
    }

    /**
     * @brief 打印内存使用信息
     */
    inline void printMemoryUsage(const std::string& label)
    {
        auto mem = getMemoryUsage();
        std::cout << "[MEMORY] " << label << ": "
                  << std::fixed << std::setprecision(2)
                  << mem.currentMB() << " MB (peak: " << mem.peakMB() << " MB)"
                  << std::endl;
    }

    /**
     * @brief 内存变化追踪器
     */
    class MemoryTracker
    {
    private:
        std::string name_;
        MemoryInfo startMem_;

    public:
        explicit MemoryTracker(std::string name)
            : name_(std::move(name))
            , startMem_(getMemoryUsage())
        {
        }

        ~MemoryTracker()
        {
            report();
        }

        void report()
        {
            auto endMem = getMemoryUsage();
            auto deltaBytes = static_cast<std::int64_t>(endMem.currentBytes) -
                             static_cast<std::int64_t>(startMem_.currentBytes);
            double deltaMB = static_cast<double>(deltaBytes) / (1024.0 * 1024.0);

            std::cout << "[MEMORY] " << name_ << ": delta = "
                      << std::fixed << std::setprecision(2)
                      << (deltaBytes >= 0 ? "+" : "") << deltaMB << " MB, "
                      << "current = " << endMem.currentMB() << " MB, "
                      << "peak = " << endMem.peakMB() << " MB"
                      << std::endl;
        }

        // 禁用拷贝
        MemoryTracker(const MemoryTracker&) = delete;
        MemoryTracker& operator=(const MemoryTracker&) = delete;
    };

} // namespace TestUtils

// ============================================================
// 压力测试配置
// ============================================================

struct StressTestConfig
{
    // 大数据量配置
    static constexpr int STUDENT_COUNT = 1000;
    static constexpr int GROUP_COUNT = 100;
    static constexpr int EVENT_COUNT = 5'000'000;  // 500万条记录
    static constexpr int STUDENT_BASE_ID = 10000;
    static constexpr int GROUP_BASE_ID = 20000;

    // 进度报告间隔
    static constexpr int PROGRESS_INTERVAL = 100'000;  // 每10万条报告一次
};

// ============================================================
// 压力测试
// ============================================================

class StressTest : public ::testing::Test
{
protected:
    std::filesystem::path testDbPath{"./test_stress_gtest"};
    SchemaDef studentSchema;
    SchemaDef groupSchema;

    void SetUp() override
    {
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);

        studentSchema = {
            {"name", FieldType::String},
            {"age", FieldType::Int},
            {"score", FieldType::Double}
        };

        groupSchema = {
            {"title", FieldType::String},
            {"level", FieldType::Int}
        };

        std::cout << "\n========== Stress Test Setup ==========" << std::endl;
        TestUtils::printMemoryUsage("Initial");
    }

    void TearDown() override
    {
        std::cout << "========================================\n" << std::endl;
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
    }

    // 辅助函数
    static std::string studentName(int id) { return "Student-" + std::to_string(id); }
    static int studentAge(int id) { return 18 + (id % 10); }
    static double studentScore(int id) { return 60.0 + static_cast<double>(id % 40); }
    static std::string groupTitle(int id) { return "Group-" + std::to_string(id); }
    static int groupLevel(int id) { return 1 + (id % 5); }
};

TEST_F(StressTest, BulkStudentCreation)
{
    using namespace TestUtils;
    constexpr int count = StressTestConfig::STUDENT_COUNT;

    std::cout << "\n--- Bulk Student Creation (" << count << " students) ---" << std::endl;

    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    {
        MemoryTracker memTracker("Student creation");
        Timer timer("Create " + std::to_string(count) + " students");

        for (int i = 0; i < count; ++i)
        {
            int id = StressTestConfig::STUDENT_BASE_ID + i;
            auto student = db.createStudent(id);
            student["name"] = studentName(id);
            student["age"] = studentAge(id);
            student["score"] = studentScore(id);
        }
    }

    EXPECT_EQ(db.students().size(), count);
    printMemoryUsage("After creation");
}

TEST_F(StressTest, BulkGroupCreation)
{
    using namespace TestUtils;
    constexpr int count = StressTestConfig::GROUP_COUNT;

    std::cout << "\n--- Bulk Group Creation (" << count << " groups) ---" << std::endl;

    SecScoreDB db(testDbPath);
    db.initGroupSchema(groupSchema);

    {
        MemoryTracker memTracker("Group creation");
        Timer timer("Create " + std::to_string(count) + " groups");

        for (int i = 0; i < count; ++i)
        {
            int id = StressTestConfig::GROUP_BASE_ID + i;
            auto group = db.createGroup(id);
            group["title"] = groupTitle(id);
            group["level"] = groupLevel(id);
        }
    }

    EXPECT_EQ(db.groups().size(), count);
    printMemoryUsage("After creation");
}

TEST_F(StressTest, BulkEventGeneration)
{
    using namespace TestUtils;
    constexpr int eventCount = StressTestConfig::EVENT_COUNT;
    constexpr int progressInterval = StressTestConfig::PROGRESS_INTERVAL;

    std::cout << "\n--- Bulk Event Generation (" << eventCount / 1'000'000.0 << "M events) ---" << std::endl;

    SecScoreDB db(testDbPath);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> targetDist(
        StressTestConfig::STUDENT_BASE_ID,
        StressTestConfig::STUDENT_BASE_ID + StressTestConfig::STUDENT_COUNT - 1
    );
    std::uniform_int_distribution<int> deltaDist(-20, 20);
    std::uniform_int_distribution<int> typeDist(0, 1);

    {
        MemoryTracker memTracker("Event generation");
        Timer timer("Generate " + std::to_string(eventCount) + " events");

        auto lastReportTime = std::chrono::steady_clock::now();

        for (int i = 0; i < eventCount; ++i)
        {
            EventType type = typeDist(rng) == 0 ? EventType::Student : EventType::Group;
            Event evt(INVALID_ID, type, targetDist(rng),
                     "Event #" + std::to_string(i), 1, deltaDist(rng));
            db.addEvent(std::move(evt));

            // 进度报告
            if ((i + 1) % progressInterval == 0)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration<double>(now - lastReportTime).count();
                double rate = progressInterval / elapsed;

                std::cout << "  Progress: " << (i + 1) / 1'000'000.0 << "M / "
                          << eventCount / 1'000'000.0 << "M events ("
                          << std::fixed << std::setprecision(0) << rate << " events/sec)"
                          << std::endl;

                printMemoryUsage("  Current");
                lastReportTime = now;
            }
        }
    }

    auto allEvents = db.getEvents([](const Event&) { return true; });
    EXPECT_EQ(allEvents.size(), eventCount);
    printMemoryUsage("After generation");
}

TEST_F(StressTest, QueryPerformance)
{
    using namespace TestUtils;
    constexpr int studentCount = StressTestConfig::STUDENT_COUNT;

    std::cout << "\n--- Query Performance ---" << std::endl;

    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    // 创建数据
    {
        Timer timer("Setup: Create " + std::to_string(studentCount) + " students");
        for (int i = 0; i < studentCount; ++i)
        {
            int id = StressTestConfig::STUDENT_BASE_ID + i;
            auto student = db.createStudent(id);
            student["name"] = studentName(id);
            student["age"] = studentAge(id);
            student["score"] = studentScore(id);
        }
    }

    // 单个查询
    {
        Timer timer("Query single student by ID (1000 times)");
        for (int i = 0; i < 1000; ++i)
        {
            int id = StressTestConfig::STUDENT_BASE_ID + (i % studentCount);
            auto student = db.getStudent(id);
            (void)student; // 防止优化
        }
    }

    // 条件查询
    {
        Timer timer("Query students by predicate (age >= 23)");
        auto results = db.getStudent([](auto& s) {
            return static_cast<int>(s["age"]) >= 23;
        });
        std::cout << "  Found: " << results.size() << " students" << std::endl;
    }

    // 条件查询 - 分数范围
    {
        Timer timer("Query students by predicate (score > 80)");
        auto results = db.getStudent([](auto& s) {
            return static_cast<double>(s["score"]) > 80.0;
        });
        std::cout << "  Found: " << results.size() << " students" << std::endl;
    }

    printMemoryUsage("After queries");
}

TEST_F(StressTest, PersistencePerformance)
{
    using namespace TestUtils;
    constexpr int studentCount = 500;
    constexpr int eventCount = 5000;

    std::cout << "\n--- Persistence Performance ---" << std::endl;

    // 创建并保存
    {
        SecScoreDB db(testDbPath);
        db.initStudentSchema(studentSchema);

        {
            Timer timer("Create " + std::to_string(studentCount) + " students");
            for (int i = 0; i < studentCount; ++i)
            {
                int id = StressTestConfig::STUDENT_BASE_ID + i;
                auto student = db.createStudent(id);
                student["name"] = studentName(id);
                student["age"] = studentAge(id);
                student["score"] = studentScore(id);
            }
        }

        {
            Timer timer("Create " + std::to_string(eventCount) + " events");
            for (int i = 0; i < eventCount; ++i)
            {
                Event evt(INVALID_ID, EventType::Student, 1000 + (i % 500),
                         "Event " + std::to_string(i), 1, i % 20 - 10);
                db.addEvent(std::move(evt));
            }
        }

        {
            Timer timer("Commit to disk");
            db.commit();
        }

        printMemoryUsage("Before close");
    }

    // 重新加载
    {
        Timer timer("Reload database from disk");
        SecScoreDB db(testDbPath);
        db.initStudentSchema(studentSchema);

        EXPECT_EQ(db.students().size(), studentCount);

        auto events = db.getEvents([](const Event&) { return true; });
        EXPECT_EQ(events.size(), eventCount);

        printMemoryUsage("After reload");
    }
}

TEST_F(StressTest, DeletePerformance)
{
    using namespace TestUtils;
    constexpr int studentCount = 1000;

    std::cout << "\n--- Delete Performance ---" << std::endl;

    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    // 创建数据
    {
        Timer timer("Setup: Create " + std::to_string(studentCount) + " students");
        for (int i = 0; i < studentCount; ++i)
        {
            int id = StressTestConfig::STUDENT_BASE_ID + i;
            auto student = db.createStudent(id);
            student["name"] = studentName(id);
            student["age"] = studentAge(id);
            student["score"] = studentScore(id);
        }
    }

    printMemoryUsage("Before delete");

    // 按条件删除
    {
        Timer timer("Delete students by predicate (score < 70)");
        std::size_t deleted = db.deleteStudent([](auto& s) {
            return static_cast<double>(s["score"]) < 70.0;
        });
        std::cout << "  Deleted: " << deleted << " students" << std::endl;
    }

    printMemoryUsage("After delete");

    // 验证
    EXPECT_LT(db.students().size(), studentCount);
}

TEST_F(StressTest, ConcurrentOperations)
{
    using namespace TestUtils;

    std::cout << "\n--- Mixed Operations Performance ---" << std::endl;

    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);
    db.initGroupSchema(groupSchema);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> opDist(0, 2);  // 0=create, 1=query, 2=delete
    std::uniform_int_distribution<int> idDist(0, 999);

    constexpr int operationCount = 5000;
    int creates = 0, queries = 0, deletes = 0;

    // 先创建一些基础数据
    for (int i = 0; i < 500; ++i)
    {
        int id = StressTestConfig::STUDENT_BASE_ID + i;
        auto student = db.createStudent(id);
        student["name"] = studentName(id);
        student["age"] = studentAge(id);
        student["score"] = studentScore(id);
    }

    {
        Timer timer("Execute " + std::to_string(operationCount) + " mixed operations");

        for (int i = 0; i < operationCount; ++i)
        {
            int op = opDist(rng);
            int targetId = StressTestConfig::STUDENT_BASE_ID + idDist(rng);

            switch (op)
            {
                case 0: // Create
                    if (!db.hasStudent(targetId))
                    {
                        auto s = db.createStudent(targetId);
                        s["name"] = studentName(targetId);
                        s["age"] = studentAge(targetId);
                        s["score"] = studentScore(targetId);
                        ++creates;
                    }
                    break;

                case 1: // Query
                    if (db.hasStudent(targetId))
                    {
                        auto s = db.getStudent(targetId);
                        (void)static_cast<std::string>(s["name"]);
                        ++queries;
                    }
                    break;

                case 2: // Delete
                    if (db.deleteStudent(targetId))
                    {
                        ++deletes;
                    }
                    break;
            }
        }
    }

    std::cout << "  Operations breakdown: "
              << "creates=" << creates << ", "
              << "queries=" << queries << ", "
              << "deletes=" << deletes << std::endl;

    printMemoryUsage("After mixed operations");
}

// ============================================================
// 内存泄漏检测测试
// ============================================================

TEST_F(StressTest, MemoryLeakCheck)
{
    using namespace TestUtils;

    std::cout << "\n--- Memory Leak Check ---" << std::endl;

    auto initialMem = getMemoryUsage();
    std::cout << "Initial memory: " << initialMem.currentMB() << " MB" << std::endl;

    // 执行多次创建和销毁循环
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        {
            SecScoreDB db(testDbPath);
            db.initStudentSchema(studentSchema);

            for (int i = 0; i < 200; ++i)
            {
                auto s = db.createStudent(StressTestConfig::STUDENT_BASE_ID + i);
                s["name"] = studentName(i);
                s["age"] = studentAge(i);
                s["score"] = studentScore(i);
            }

            // DB 在这里销毁
        }

        // 清理测试目录
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
    }

    auto finalMem = getMemoryUsage();
    std::cout << "Final memory: " << finalMem.currentMB() << " MB" << std::endl;

    auto deltaBytes = static_cast<std::int64_t>(finalMem.currentBytes) -
                     static_cast<std::int64_t>(initialMem.currentBytes);
    double deltaMB = static_cast<double>(deltaBytes) / (1024.0 * 1024.0);

    std::cout << "Memory delta after 5 cycles: "
              << (deltaBytes >= 0 ? "+" : "") << deltaMB << " MB" << std::endl;

    // 允许小幅度的内存增长（可能是系统缓存等）
    // 但大的泄漏应该被检测到
    EXPECT_LT(deltaMB, 50.0) << "Potential memory leak detected!";
}

// ============================================================
// 完整大数据集成测试：1000学生 + 500万事件
// ============================================================

TEST_F(StressTest, FullScaleIntegrationTest)
{
    using namespace TestUtils;

    constexpr int studentCount = StressTestConfig::STUDENT_COUNT;  // 1000
    constexpr int groupCount = StressTestConfig::GROUP_COUNT;      // 100
    constexpr int eventCount = StressTestConfig::EVENT_COUNT;      // 500万
    constexpr int progressInterval = StressTestConfig::PROGRESS_INTERVAL;

    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║     FULL SCALE INTEGRATION TEST                          ║\n"
              << "║     Students: " << std::setw(6) << studentCount << "                                     ║\n"
              << "║     Groups:   " << std::setw(6) << groupCount << "                                     ║\n"
              << "║     Events:   " << std::setw(6) << eventCount / 1'000'000.0 << "M                                    ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << std::endl;

    printMemoryUsage("Initial state");
    auto totalStartTime = std::chrono::steady_clock::now();

    // ========== Phase 1: 创建数据库和 Schema ==========
    std::cout << "\n[Phase 1] Creating database and initializing schemas..." << std::endl;

    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);
    db.initGroupSchema(groupSchema);

    // ========== Phase 2: 创建分组 ==========
    std::cout << "\n[Phase 2] Creating " << groupCount << " groups..." << std::endl;
    {
        MemoryTracker memTracker("Group creation");
        Timer timer("Create groups");

        for (int i = 0; i < groupCount; ++i)
        {
            int id = StressTestConfig::GROUP_BASE_ID + i;
            auto group = db.createGroup(id);
            group["title"] = groupTitle(id);
            group["level"] = groupLevel(id);
        }
    }
    EXPECT_EQ(db.groups().size(), groupCount);

    // ========== Phase 3: 创建学生 ==========
    std::cout << "\n[Phase 3] Creating " << studentCount << " students..." << std::endl;
    {
        MemoryTracker memTracker("Student creation");
        Timer timer("Create students");

        for (int i = 0; i < studentCount; ++i)
        {
            int id = StressTestConfig::STUDENT_BASE_ID + i;
            auto student = db.createStudent(id);
            student["name"] = studentName(id);
            student["age"] = studentAge(id);
            student["score"] = studentScore(id);
        }
    }
    EXPECT_EQ(db.students().size(), studentCount);
    printMemoryUsage("After students/groups creation");

    // ========== Phase 4: 生成500万事件 ==========
    std::cout << "\n[Phase 4] Generating " << eventCount / 1'000'000.0 << " million events..." << std::endl;
    std::cout << "  (Progress will be reported every " << progressInterval / 1000 << "K events)" << std::endl;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> studentIdDist(
        StressTestConfig::STUDENT_BASE_ID,
        StressTestConfig::STUDENT_BASE_ID + studentCount - 1
    );
    std::uniform_int_distribution<int> groupIdDist(
        StressTestConfig::GROUP_BASE_ID,
        StressTestConfig::GROUP_BASE_ID + groupCount - 1
    );
    std::uniform_int_distribution<int> deltaDist(-50, 50);
    std::uniform_int_distribution<int> typeDist(0, 99);  // 70% student, 30% group
    std::uniform_int_distribution<int> operatorDist(1, 100);

    std::size_t studentEventCount = 0;
    std::size_t groupEventCount = 0;
    std::size_t erasedEventCount = 0;

    {
        MemoryTracker memTracker("Event generation (5M)");
        Timer timer("Generate 5 million events");

        auto phaseStartTime = std::chrono::steady_clock::now();
        auto lastReportTime = phaseStartTime;

        for (int i = 0; i < eventCount; ++i)
        {
            bool isStudentEvent = typeDist(rng) < 70;
            int targetId = isStudentEvent ? studentIdDist(rng) : groupIdDist(rng);
            EventType type = isStudentEvent ? EventType::Student : EventType::Group;

            Event evt(INVALID_ID, type, targetId,
                     "Adjustment #" + std::to_string(i),
                     operatorDist(rng), deltaDist(rng));

            int assignedId = db.addEvent(std::move(evt));

            if (isStudentEvent)
            {
                ++studentEventCount;
            }
            else
            {
                ++groupEventCount;
            }

            // 每10条事件擦除1条（模拟撤销操作）
            if (i % 10 == 0)
            {
                db.setEventErased(assignedId, true);
                ++erasedEventCount;
            }

            // 进度报告
            if ((i + 1) % progressInterval == 0)
            {
                auto now = std::chrono::steady_clock::now();
                auto intervalElapsed = std::chrono::duration<double>(now - lastReportTime).count();
                auto totalElapsed = std::chrono::duration<double>(now - phaseStartTime).count();

                double currentRate = progressInterval / intervalElapsed;
                double avgRate = (i + 1) / totalElapsed;
                double remaining = (eventCount - i - 1) / avgRate;

                std::cout << "  [" << std::fixed << std::setprecision(1)
                          << ((i + 1) * 100.0 / eventCount) << "%] "
                          << (i + 1) / 1'000'000.0 << "M / " << eventCount / 1'000'000.0 << "M | "
                          << std::setprecision(0) << currentRate << " evt/s | "
                          << "ETA: " << std::setprecision(1) << remaining << "s"
                          << std::endl;

                printMemoryUsage("    Memory");
                lastReportTime = now;
            }
        }
    }

    std::cout << "\n  Event statistics:" << std::endl;
    std::cout << "    Student events: " << studentEventCount << std::endl;
    std::cout << "    Group events:   " << groupEventCount << std::endl;
    std::cout << "    Erased events:  " << erasedEventCount << std::endl;

    // ========== Phase 5: 验证数据 ==========
    std::cout << "\n[Phase 5] Verifying data..." << std::endl;

    {
        Timer timer("Count all events");
        auto allEvents = db.getEvents([](const Event&) { return true; });
        EXPECT_EQ(allEvents.size(), eventCount);
        std::cout << "  Total events: " << allEvents.size() << std::endl;
    }

    {
        Timer timer("Count erased events");
        auto erasedEvents = db.getEvents([](const Event& e) { return e.IsErased(); });
        EXPECT_EQ(erasedEvents.size(), erasedEventCount);
        std::cout << "  Erased events: " << erasedEvents.size() << std::endl;
    }

    {
        Timer timer("Query student events");
        auto studentEvents = db.getEvents([](const Event& e) {
            return e.GetEventType() == EventType::Student;
        });
        EXPECT_EQ(studentEvents.size(), studentEventCount);
        std::cout << "  Student events: " << studentEvents.size() << std::endl;
    }

    // ========== Phase 6: 持久化测试 ==========
    std::cout << "\n[Phase 6] Persistence test..." << std::endl;

    {
        Timer timer("Commit to disk");
        db.commit();
    }

    // 获取文件大小
    std::size_t totalFileSize = 0;
    for (const auto& entry : std::filesystem::directory_iterator(testDbPath))
    {
        if (entry.is_regular_file())
        {
            totalFileSize += entry.file_size();
            std::cout << "  " << entry.path().filename().string() << ": "
                      << std::fixed << std::setprecision(2)
                      << entry.file_size() / (1024.0 * 1024.0) << " MB" << std::endl;
        }
    }
    std::cout << "  Total disk usage: " << totalFileSize / (1024.0 * 1024.0) << " MB" << std::endl;

    printMemoryUsage("Before closing database");

    // ========== Phase 7: 重新加载测试 ==========
    std::cout << "\n[Phase 7] Reload test..." << std::endl;

    // 手动调用析构
    // 注意：我们需要在作用域外重新创建 db，但这里为了简化，跳过实际重载测试
    // 完整的重载测试在 PersistencePerformance 中

    // ========== 总结 ==========
    auto totalEndTime = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration<double>(totalEndTime - totalStartTime).count();

    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║     TEST COMPLETE                                        ║\n"
              << "╠══════════════════════════════════════════════════════════╣\n"
              << "║  Total time:    " << std::setw(8) << std::fixed << std::setprecision(2)
              << totalDuration << " seconds                       ║\n"
              << "║  Students:      " << std::setw(8) << studentCount << "                               ║\n"
              << "║  Groups:        " << std::setw(8) << groupCount << "                               ║\n"
              << "║  Events:        " << std::setw(8) << eventCount << "                               ║\n"
              << "║  Throughput:    " << std::setw(8) << std::setprecision(0)
              << (eventCount / totalDuration) << " events/sec                    ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << std::endl;

    printMemoryUsage("Final state");
}

// ============================================================
// 大数据持久化和重载测试
// ============================================================

TEST_F(StressTest, LargeDataPersistenceReload)
{
    using namespace TestUtils;

    // 使用较小的事件数量进行持久化测试（避免测试时间过长）
    constexpr int studentCount = StressTestConfig::STUDENT_COUNT;
    constexpr int eventCount = 100'000;  // 10万条用于持久化测试

    std::cout << "\n--- Large Data Persistence & Reload Test ---" << std::endl;
    std::cout << "  Students: " << studentCount << ", Events: " << eventCount << std::endl;

    // 创建并保存
    {
        std::cout << "\n[Step 1] Creating and saving data..." << std::endl;

        SecScoreDB db(testDbPath);
        db.initStudentSchema(studentSchema);
        db.initGroupSchema(groupSchema);

        {
            Timer timer("Create students");
            for (int i = 0; i < studentCount; ++i)
            {
                int id = StressTestConfig::STUDENT_BASE_ID + i;
                auto student = db.createStudent(id);
                student["name"] = studentName(id);
                student["age"] = studentAge(id);
                student["score"] = studentScore(id);
            }
        }

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> targetDist(
            StressTestConfig::STUDENT_BASE_ID,
            StressTestConfig::STUDENT_BASE_ID + studentCount - 1
        );
        std::uniform_int_distribution<int> deltaDist(-20, 20);

        {
            Timer timer("Create events");
            for (int i = 0; i < eventCount; ++i)
            {
                Event evt(INVALID_ID, EventType::Student, targetDist(rng),
                         "Event " + std::to_string(i), 1, deltaDist(rng));
                db.addEvent(std::move(evt));
            }
        }

        printMemoryUsage("Before commit");

        {
            Timer timer("Commit to disk");
            db.commit();
        }

        printMemoryUsage("After commit (before close)");
    }

    // 重新加载
    {
        std::cout << "\n[Step 2] Reloading data from disk..." << std::endl;

        MemoryTracker memTracker("Database reload");
        Timer timer("Reload database");

        SecScoreDB db(testDbPath);
        db.initStudentSchema(studentSchema);
        db.initGroupSchema(groupSchema);

        timer.stop();

        EXPECT_EQ(db.students().size(), studentCount);

        auto events = db.getEvents([](const Event&) { return true; });
        EXPECT_EQ(events.size(), eventCount);

        // 验证数据完整性
        std::cout << "\n[Step 3] Verifying data integrity..." << std::endl;

        // 随机抽样验证
        std::mt19937 rng(123);
        std::uniform_int_distribution<int> sampleDist(0, studentCount - 1);

        bool allValid = true;
        for (int i = 0; i < 10; ++i)
        {
            int idx = sampleDist(rng);
            int id = StressTestConfig::STUDENT_BASE_ID + idx;

            auto student = db.getStudent(id);
            std::string expectedName = studentName(id);
            std::string actualName = static_cast<std::string>(student["name"]);

            if (actualName != expectedName)
            {
                std::cout << "  [FAIL] Student " << id << ": expected '"
                          << expectedName << "', got '" << actualName << "'" << std::endl;
                allValid = false;
            }
        }

        if (allValid)
        {
            std::cout << "  [PASS] All sampled data verified correctly" << std::endl;
        }

        EXPECT_TRUE(allValid);
        printMemoryUsage("After verification");
    }
}

