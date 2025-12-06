/**
 * @file entity_test.cpp
 * @brief Student, Group, Event 实体单元测试
 */
#include <gtest/gtest.h>
#include "Student.h"
#include "Group.h"
#include "Event.h"

using namespace SSDB;

// ============================================================
// Student 测试
// ============================================================

class StudentTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StudentTest, DefaultConstruction)
{
    Student s;
    EXPECT_EQ(s.GetId(), 0);
    EXPECT_TRUE(s.GetGroups().empty());
    EXPECT_TRUE(s.GetMetadata().empty());
}

TEST_F(StudentTest, ParameterizedConstruction)
{
    std::vector<int> groups = {1, 2, 3};
    t_metadata meta = {{"name", "Alice"}, {"age", "20"}};

    Student s(100, groups, meta);

    EXPECT_EQ(s.GetId(), 100);
    EXPECT_EQ(s.GetGroups().size(), 3);
    EXPECT_EQ(s.GetMetadata().size(), 2);
}

TEST_F(StudentTest, SettersAndGetters)
{
    Student s;
    s.SetId(42);
    EXPECT_EQ(s.GetId(), 42);

    s.SetGroups({10, 20});
    EXPECT_EQ(s.GetGroups().size(), 2);

    s.AddGroup(30);
    EXPECT_EQ(s.GetGroups().size(), 3);
}

TEST_F(StudentTest, MetadataOperations)
{
    Student s;
    s.SetMetadataValue("name", "Bob");
    s.SetMetadataValue("score", "95");

    EXPECT_EQ(s.GetMetadataValue("name"), "Bob");
    EXPECT_EQ(s.GetMetadataValue("score"), "95");
    EXPECT_EQ(s.GetMetadataValue("nonexistent"), "");
}

TEST_F(StudentTest, GroupMembership)
{
    Student s;
    s.SetGroups({1, 2, 3});

    EXPECT_TRUE(s.BelongsToGroup(1));
    EXPECT_TRUE(s.BelongsToGroup(2));
    EXPECT_TRUE(s.BelongsToGroup(3));
    EXPECT_FALSE(s.BelongsToGroup(4));
}

TEST_F(StudentTest, RemoveGroup)
{
    Student s;
    s.SetGroups({1, 2, 3});

    EXPECT_TRUE(s.RemoveGroup(2));
    EXPECT_FALSE(s.BelongsToGroup(2));
    EXPECT_EQ(s.GetGroups().size(), 2);

    EXPECT_FALSE(s.RemoveGroup(99)); // 不存在的组
}

// ============================================================
// Group 测试
// ============================================================

class GroupTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GroupTest, DefaultConstruction)
{
    Group g;
    EXPECT_EQ(g.GetId(), 0);
    EXPECT_TRUE(g.GetStudents().empty());
    EXPECT_TRUE(g.GetMetadata().empty());
}

TEST_F(GroupTest, ParameterizedConstruction)
{
    std::vector<int> students = {101, 102, 103};
    t_metadata meta = {{"title", "Class A"}};

    Group g(1, students, meta);

    EXPECT_EQ(g.GetId(), 1);
    EXPECT_EQ(g.GetStudents().size(), 3);
    EXPECT_EQ(g.GetMetadataValue("title"), "Class A");
}

TEST_F(GroupTest, StudentOperations)
{
    Group g;
    g.AddStudent(101);
    g.AddStudent(102);

    EXPECT_EQ(g.StudentCount(), 2);
    EXPECT_TRUE(g.ContainsStudent(101));
    EXPECT_TRUE(g.ContainsStudent(102));
    EXPECT_FALSE(g.ContainsStudent(103));
}

TEST_F(GroupTest, RemoveStudent)
{
    Group g;
    g.SetStudents({101, 102, 103});

    EXPECT_TRUE(g.RemoveStudent(102));
    EXPECT_FALSE(g.ContainsStudent(102));
    EXPECT_EQ(g.StudentCount(), 2);

    EXPECT_FALSE(g.RemoveStudent(999)); // 不存在的学生
}

TEST_F(GroupTest, MetadataOperations)
{
    Group g;
    g.SetMetadataValue("title", "Math Class");
    g.SetMetadataValue("level", "3");

    EXPECT_EQ(g.GetMetadataValue("title"), "Math Class");
    EXPECT_EQ(g.GetMetadataValue("level"), "3");
}

// ============================================================
// Event 测试
// ============================================================

class EventTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EventTest, DefaultConstruction)
{
    Event e;
    EXPECT_EQ(e.GetId(), INVALID_ID);
    EXPECT_EQ(e.GetEventType(), EventType::Student);
    EXPECT_EQ(e.GetOperatingObject(), 0);
    EXPECT_TRUE(e.GetReason().empty());
    EXPECT_EQ(e.GetOperatePerson(), 0);
    EXPECT_EQ(e.GetDeltaScore(), 0);
    EXPECT_FALSE(e.IsErased());
}

TEST_F(EventTest, ParameterizedConstruction)
{
    Event e(1, EventType::Group, 100, "Test reason", 42, 10);

    EXPECT_EQ(e.GetId(), 1);
    EXPECT_EQ(e.GetEventType(), EventType::Group);
    EXPECT_EQ(e.GetOperatingObject(), 100);
    EXPECT_EQ(e.GetReason(), "Test reason");
    EXPECT_EQ(e.GetOperatePerson(), 42);
    EXPECT_EQ(e.GetDeltaScore(), 10);
    EXPECT_FALSE(e.IsErased());
}

TEST_F(EventTest, SettersAndGetters)
{
    Event e;

    e.SetId(99);
    EXPECT_EQ(e.GetId(), 99);

    e.SetEventType(EventType::Group);
    EXPECT_EQ(e.GetEventType(), EventType::Group);

    e.SetOperatingObject(200);
    EXPECT_EQ(e.GetOperatingObject(), 200);

    e.SetReason("Updated reason");
    EXPECT_EQ(e.GetReason(), "Updated reason");

    e.SetOperatePerson(50);
    EXPECT_EQ(e.GetOperatePerson(), 50);

    e.SetDeltaScore(-5);
    EXPECT_EQ(e.GetDeltaScore(), -5);

    e.SetErased(true);
    EXPECT_TRUE(e.IsErased());
}

TEST_F(EventTest, TimeOperations)
{
    Event e;
    auto now = std::chrono::system_clock::now();

    e.SetEventTime(now);
    EXPECT_EQ(e.GetEventTime(), now);
}

TEST_F(EventTest, InvalidIdConstant)
{
    EXPECT_EQ(INVALID_ID, -1);

    // 测试 constexpr
    constexpr int invalidId = INVALID_ID;
    EXPECT_EQ(invalidId, -1);
}
an