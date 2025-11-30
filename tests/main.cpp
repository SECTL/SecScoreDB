#include "SecScoreDB.h"
#include <array>
#include <cassert>
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
                      studentEvent ? EventType::STUDENT : EventType::GROUP,
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
        SecScoreDB db(dbPath);
        db.initStudentSchema(stuSchema);
        db.initGroupSchema(grpSchema);

        seedGroups(db);
        seedStudents(db);
        emittedStats = emitEvents(db);

        db.commit();
        logPhase("Initial commit complete.");
    }

    {
        logPhase("Re-opening database for verification...");
        SecScoreDB db(dbPath);
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

        auto totalEvents = db.getEvents([](const Event&) { return true; });
        auto erasedEvents = db.getEvents([](const Event& e) { return e.IsErased(); });
        size_t totalCount = totalEvents.size();
        size_t erasedCount = erasedEvents.size();

        std::cout << "[VERIFY] Total events stored: " << totalCount << std::endl;
        std::cout << "[VERIFY] Erased events stored: " << erasedCount << std::endl;
        std::cout << "[VERIFY] Expected totals: " << emittedStats.total
            << ", expected erased: " << emittedStats.erasedEvents << std::endl;

        assert(totalCount == emittedStats.total);
        assert(erasedCount == emittedStats.erasedEvents);

        std::cout << "[TEST] Verification completed successfully." << std::endl;
    }

    std::cout << "[TEST] SecScoreDB stress test finished" << std::endl;
    return 0;
}
