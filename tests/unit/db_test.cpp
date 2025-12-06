/**
 * @file db_test.cpp
 * @brief SecScoreDB 集成测试
 */
#include <gtest/gtest.h>
#include "SecScoreDB.h"
#include <filesystem>

using namespace SSDB;

// ============================================================
// SecScoreDB 集成测试
// ============================================================

class SecScoreDBTest : public ::testing::Test
{
protected:
    std::filesystem::path testDbPath{"./test_db_gtest"};
    SchemaDef studentSchema;
    SchemaDef groupSchema;

    void SetUp() override
    {
        // 清理旧的测试目录
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);

        // 设置 Schema
        studentSchema = {
            {"name", FieldType::String},
            {"age", FieldType::Int},
            {"score", FieldType::Double}
        };

        groupSchema = {
            {"title", FieldType::String},
            {"level", FieldType::Int}
        };
    }

    void TearDown() override
    {
        // 清理测试目录
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
    }
};

TEST_F(SecScoreDBTest, CreateDatabaseDirectory)
{
    SecScoreDB db(testDbPath);
    EXPECT_TRUE(std::filesystem::exists(testDbPath));
}

TEST_F(SecScoreDBTest, InitializeSchemas)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);
    db.initGroupSchema(groupSchema);

    EXPECT_EQ(db.studentSchema().size(), 3);
    EXPECT_EQ(db.groupSchema().size(), 2);
}

TEST_F(SecScoreDBTest, CreateStudent)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    auto student = db.createStudent(1001);
    student["name"] = std::string("Alice");
    student["age"] = 20;
    student["score"] = 95.5;

    EXPECT_TRUE(db.hasStudent(1001));
    EXPECT_EQ(db.students().size(), 1);
}

TEST_F(SecScoreDBTest, GetStudent)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    auto created = db.createStudent(1001);
    created["name"] = std::string("Bob");
    created["age"] = 21;
    created["score"] = 88.0;

    auto retrieved = db.getStudent(1001);
    EXPECT_EQ(static_cast<std::string>(retrieved["name"]), "Bob");
    EXPECT_EQ(static_cast<int>(retrieved["age"]), 21);
    EXPECT_EQ(static_cast<double>(retrieved["score"]), 88.0);
}

TEST_F(SecScoreDBTest, CreateStudentDuplicateIdThrows)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    db.createStudent(1001);
    EXPECT_THROW(db.createStudent(1001), std::runtime_error);
}

TEST_F(SecScoreDBTest, GetNonExistentStudentThrows)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    EXPECT_THROW(db.getStudent(9999), std::runtime_error);
}

TEST_F(SecScoreDBTest, DeleteStudent)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    db.createStudent(1001);
    EXPECT_TRUE(db.hasStudent(1001));

    bool deleted = db.deleteStudent(1001);
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(db.hasStudent(1001));
}

TEST_F(SecScoreDBTest, DeleteNonExistentStudent)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    bool deleted = db.deleteStudent(9999);
    EXPECT_FALSE(deleted);
}

TEST_F(SecScoreDBTest, CreateGroup)
{
    SecScoreDB db(testDbPath);
    db.initGroupSchema(groupSchema);

    auto group = db.createGroup(2001);
    group["title"] = std::string("Class A");
    group["level"] = 3;

    EXPECT_TRUE(db.hasGroup(2001));
    EXPECT_EQ(db.groups().size(), 1);
}

TEST_F(SecScoreDBTest, GetGroup)
{
    SecScoreDB db(testDbPath);
    db.initGroupSchema(groupSchema);

    auto created = db.createGroup(2001);
    created["title"] = std::string("Class B");
    created["level"] = 2;

    auto retrieved = db.getGroup(2001);
    EXPECT_EQ(static_cast<std::string>(retrieved["title"]), "Class B");
    EXPECT_EQ(static_cast<int>(retrieved["level"]), 2);
}

TEST_F(SecScoreDBTest, DeleteGroup)
{
    SecScoreDB db(testDbPath);
    db.initGroupSchema(groupSchema);

    db.createGroup(2001);
    EXPECT_TRUE(db.hasGroup(2001));

    bool deleted = db.deleteGroup(2001);
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(db.hasGroup(2001));
}

TEST_F(SecScoreDBTest, AddEvent)
{
    SecScoreDB db(testDbPath);

    Event e(INVALID_ID, EventType::Student, 1001, "Test event", 1, 10);
    int eventId = db.addEvent(std::move(e));

    EXPECT_GT(eventId, 0);
}

TEST_F(SecScoreDBTest, GetEvents)
{
    SecScoreDB db(testDbPath);

    db.addEvent(Event(INVALID_ID, EventType::Student, 1001, "Event 1", 1, 10));
    db.addEvent(Event(INVALID_ID, EventType::Student, 1002, "Event 2", 1, -5));
    db.addEvent(Event(INVALID_ID, EventType::Group, 2001, "Event 3", 1, 20));

    auto allEvents = db.getEvents([](const Event&) { return true; });
    EXPECT_EQ(allEvents.size(), 3);

    auto studentEvents = db.getEvents([](const Event& e) {
        return e.GetEventType() == EventType::Student;
    });
    EXPECT_EQ(studentEvents.size(), 2);
}

TEST_F(SecScoreDBTest, SetEventErased)
{
    SecScoreDB db(testDbPath);

    int eventId = db.addEvent(Event(INVALID_ID, EventType::Student, 1001, "Test", 1, 10));

    db.setEventErased(eventId, true);

    auto erasedEvents = db.getEvents([](const Event& e) { return e.IsErased(); });
    EXPECT_EQ(erasedEvents.size(), 1);
}

TEST_F(SecScoreDBTest, AllocateIds)
{
    SecScoreDB db(testDbPath);

    int id1 = db.allocateStudentId();
    int id2 = db.allocateStudentId();
    int id3 = db.allocateStudentId();

    EXPECT_EQ(id2, id1 + 1);
    EXPECT_EQ(id3, id2 + 1);

    int gid1 = db.allocateGroupId();
    int gid2 = db.allocateGroupId();

    EXPECT_EQ(gid2, gid1 + 1);
}

TEST_F(SecScoreDBTest, PersistenceCommitAndReload)
{
    // 创建数据并提交
    {
        SecScoreDB db(testDbPath);
        db.initStudentSchema(studentSchema);

        auto student = db.createStudent(1001);
        student["name"] = std::string("Persistent");
        student["age"] = 25;
        student["score"] = 100.0;

        db.commit();
    }

    // 重新加载并验证
    {
        SecScoreDB db(testDbPath);
        db.initStudentSchema(studentSchema);

        EXPECT_TRUE(db.hasStudent(1001));

        auto student = db.getStudent(1001);
        EXPECT_EQ(static_cast<std::string>(student["name"]), "Persistent");
        EXPECT_EQ(static_cast<int>(student["age"]), 25);
        EXPECT_EQ(static_cast<double>(student["score"]), 100.0);
    }
}

TEST_F(SecScoreDBTest, QueryStudentsByPredicate)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    for (int i = 0; i < 10; ++i)
    {
        auto s = db.createStudent(1000 + i);
        s["name"] = std::string("Student" + std::to_string(i));
        s["age"] = 18 + i;
        s["score"] = 60.0 + i * 5;
    }

    // 查询年龄 >= 23 的学生
    auto results = db.getStudent([](auto& s) {
        return static_cast<int>(s["age"]) >= 23;
    });

    EXPECT_EQ(results.size(), 5); // ages 23, 24, 25, 26, 27
}

TEST_F(SecScoreDBTest, DeleteStudentsByPredicate)
{
    SecScoreDB db(testDbPath);
    db.initStudentSchema(studentSchema);

    for (int i = 0; i < 10; ++i)
    {
        auto s = db.createStudent(1000 + i);
        s["name"] = std::string("Student" + std::to_string(i));
        s["age"] = 18 + i;
        s["score"] = 60.0 + i * 5;
    }

    // 删除分数 < 75 的学生
    size_t deleted = db.deleteStudent([](auto& s) {
        return static_cast<double>(s["score"]) < 75.0;
    });

    EXPECT_EQ(deleted, 3); // scores 60, 65, 70
    EXPECT_EQ(db.students().size(), 7);
}

