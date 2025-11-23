#include "SecScoreDB.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace SSDB;

static void cleanup(const std::filesystem::path& p)
{
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

int main()
{
    std::cout << "[TEST] SecScoreDB comprehensive test start\n";
    const std::filesystem::path dbPath{"./testdata"};
    cleanup(dbPath);
    {
        SecScoreDB db(dbPath);
        // 1. Init schemas (dynamic fields)
        SchemaDef stuSchema{{"name", FieldType::String}, {"age", FieldType::Int}, {"score", FieldType::Double}};
        SchemaDef grpSchema{{"title", FieldType::String}, {"level", FieldType::Int}};
        db.initStudentSchema(stuSchema);
        db.initGroupSchema(grpSchema);

        // 2. Create & modify dynamic student
        auto s1 = db.createStudent(1001);
        s1["name"] = std::string("Alice");
        s1["age"] = 19;
        s1["score"] = 95.5;
        // 3. Create second student via struct then add
        Student rawStu;
        rawStu.SetId(1002);
        rawStu.SetMetadataValue("name", "Bob");
        rawStu.SetMetadataValue("age", "20");
        rawStu.SetMetadataValue("score", "88.0");
        auto s2 = db.addStudent(rawStu);

        // 4. Query lambda with dynamic wrapper
        auto results = db.getStudent([](auto&& w)
        {
            try
            {
                double sc = (double)w["score"];
                return sc > 90.0;
            }
            catch (...) { return false; }
        });
        assert(results.size() == 1 && "Only Alice should have score > 90");

        // 5. Group operations
        auto g1 = db.createGroup(2001);
        g1["title"] = std::string("Group-A");
        g1["level"] = 1;
        auto g2 = db.createGroup(2002);
        g2["title"] = std::string("Group-B");
        g2["level"] = 2;

        // 6. Event operations (add & erase)
        Event e1(INVALID_ID, EventType::STUDENT, 1001, "Initial score adjust", 999, 5);
        db.addEvent(e1); // auto assigned id
        Event e2(INVALID_ID, EventType::GROUP, 2001, "Group level up", 998, 10);
        db.addEvent(e2);
        // Mark first event erased
        db.setEventErased(1, true);

        auto erasedEvents = db.getEvents([](const Event& e) { return e.IsErased(); });
        assert(erasedEvents.size() == 1 && "One event should be erased");

        // 7. Delete using predicate
        size_t removed = db.deleteStudent([](auto&& w) { return (int)w["age"] > 19; }); // remove Bob age 20
        assert(removed == 1 && "Should remove Bob");

        // 8. Commit persistence via destructor
        db.commit();
    }

    // 9. Re-open DB and verify persistence
    {
        SecScoreDB db2(dbPath);
        SchemaDef stuSchema{{"name", FieldType::String}, {"age", FieldType::Int}, {"score", FieldType::Double}};
        SchemaDef grpSchema{{"title", FieldType::String}, {"level", FieldType::Int}};
        db2.initStudentSchema(stuSchema);
        db2.initGroupSchema(grpSchema);

        // Alice should exist, Bob deleted
        auto alice = db2.getStudent(1001);
        assert(static_cast<std::string>(alice["name"]) == "Alice");
        assert(static_cast<int>(alice["age"]) == 19);
        assert(static_cast<double>(alice["score"]) == 95.5);
        bool bobMissing = false;
        try { db2.getStudent(1002); }
        catch (...) { bobMissing = true; }
        assert(bobMissing && "Bob should have been deleted");

        // Events count & erase state
        auto allErased = db2.getEvents([](const Event& e) { return e.IsErased(); });
        assert(allErased.size() == 1 && "Erased event should persist");
    }
    std::cout << "[TEST] All assertions passed.\n";
    return 0;
}
