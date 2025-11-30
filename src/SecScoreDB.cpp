#include "SecScoreDB.h"
#include <algorithm>
#include <iostream>
#include <format> // C++20

namespace SSDB
{
    // ============================================================
    // 构造与析构
    // ============================================================

    SecScoreDB::SecScoreDB(const std::filesystem::path& path)
        : stu_db(path / "students.bin"), // 使用二进制后缀
          grp_db(path / "groups.bin"),
          evt_db(path / "events.bin")
    {
        // 一行代码完成加载！
        stu = stu_db.LoadAll<Student>();
        grp = grp_db.LoadAll<Group>();
        evt = evt_db.LoadAll<Event>();

        // 初始化 Event计数器
        _max_event_id = 0;
        for (const auto& [id, _] : evt)
        {
            if (id > _max_event_id)
            {
                _max_event_id = id;
            }
        }

        _max_student_id = 0;
        for (const auto& [id, _] : stu)
        {
            if (id > _max_student_id)
            {
                _max_student_id = id;
            }
        }

        _max_group_id = 0;
        for (const auto& [id, _] : grp)
        {
            if (id > _max_group_id)
            {
                _max_group_id = id;
            }
        }

        // 打印日志
        // std::cout << "Loaded " << stu.size() << " students." << std::endl;
    }

    SecScoreDB::~SecScoreDB()
    {
        try
        {
            commit(); // 析构时自动保存
        }
        catch (const std::exception& e)
        {
            // 析构函数绝对不能抛出异常，只能记录日志
            std::cerr << "[SSDB Error] Failed to save DB on exit: " << e.what() << std::endl;
        }
    }

    void SecScoreDB::commit()
    {
        // 一行代码完成保存！
        stu_db.SaveAll(stu);
        grp_db.SaveAll(grp);
        evt_db.SaveAll(evt);
    }

    // ============================================================
    // Student 相关实现
    // ============================================================

    DynamicWrapper<Student> SecScoreDB::createStudent(int id)
    {
        if (stu.contains(id))
        {
            throw std::runtime_error(std::format("Create failed: Student ID {} already exists.", id));
        }

        Student s;
        s.SetId(id);

        _max_student_id = std::max(_max_student_id, id);

        // 移动语义插入，高效
        auto [it, success] = stu.emplace(id, std::move(s));
        _max_student_id = std::max(_max_student_id, id);
        return DynamicWrapper<Student>(it->second, _stu_schema);
    }

    DynamicWrapper<Student> SecScoreDB::addStudent(Student s)
    {
        int id = s.GetId();
        if (stu.contains(id))
        {
            throw std::runtime_error(std::format("Add failed: Student ID {} already exists.", id));
        }

        auto [it, success] = stu.emplace(id, std::move(s));
        _max_student_id = std::max(_max_student_id, id);
        return DynamicWrapper<Student>(it->second, _stu_schema);
    }

    DynamicWrapper<Student> SecScoreDB::addStudent(const DynamicWrapper<Student>& s)
    {
        // 从传入的 Wrapper 中获取底层实体（复制一份）
        Student copyEntity = s.GetEntity();
        int id = copyEntity.GetId();

        if (stu.contains(id))
        {
            throw std::runtime_error(std::format("Import failed: Student ID {} already exists.", id));
        }

        auto [it, success] = stu.emplace(id, std::move(copyEntity));
        _max_student_id = std::max(_max_student_id, id);
        return DynamicWrapper<Student>(it->second, _stu_schema);
    }

    DynamicWrapper<Student> SecScoreDB::getStudent(int id)
    {
        auto it = stu.find(id);
        if (it == stu.end())
        {
            throw std::runtime_error(std::format("Student ID {} not found.", id));
        }
        return DynamicWrapper<Student>(it->second, _stu_schema);
    }

    bool SecScoreDB::deleteStudent(int id)
    {
        return stu.erase(id) > 0;
    }

    // ============================================================
    // Group 相关实现
    // ============================================================

    DynamicWrapper<Group> SecScoreDB::createGroup(int id)
    {
        if (grp.contains(id))
        {
            throw std::runtime_error(std::format("Create failed: Group ID {} already exists.", id));
        }

        Group g;
        g.SetId(id);

        _max_group_id = std::max(_max_group_id, id);

        auto [it, success] = grp.emplace(id, std::move(g));
        _max_group_id = std::max(_max_group_id, id);
        return DynamicWrapper<Group>(it->second, _grp_schema);
    }

    DynamicWrapper<Group> SecScoreDB::addGroup(Group g)
    {
        int id = g.GetId();
        if (grp.contains(id))
        {
            throw std::runtime_error(std::format("Add failed: Group ID {} already exists.", id));
        }

        auto [it, success] = grp.emplace(id, std::move(g));
        _max_group_id = std::max(_max_group_id, id);
        return DynamicWrapper<Group>(it->second, _grp_schema);
    }

    DynamicWrapper<Group> SecScoreDB::addGroup(const DynamicWrapper<Group>& g)
    {
        Group copyEntity = g.GetEntity();
        int id = copyEntity.GetId();

        if (grp.contains(id))
        {
            throw std::runtime_error(std::format("Import failed: Group ID {} already exists.", id));
        }

        auto [it, success] = grp.emplace(id, std::move(copyEntity));
        _max_group_id = std::max(_max_group_id, id);
        return DynamicWrapper<Group>(it->second, _grp_schema);
    }

    DynamicWrapper<Group> SecScoreDB::getGroup(int id)
    {
        auto it = grp.find(id);
        if (it == grp.end())
        {
            throw std::runtime_error(std::format("Group ID {} not found.", id));
        }
        return DynamicWrapper<Group>(it->second, _grp_schema);
    }

    bool SecScoreDB::deleteGroup(int id)
    {
        return grp.erase(id) > 0;
    }

    int SecScoreDB::addEvent(Event e)
    {
        int inputId = e.GetId();
        if (inputId == INVALID_ID) // 分配新的 id
        {
            _max_event_id++;
            e.SetId(_max_event_id);
        }
        else
        {
            if (evt.contains(inputId))
            {
                throw std::runtime_error(std::format("Add Event using ID {} failed: ID already exists.", inputId));
            }
            _max_event_id = (inputId > _max_event_id ? inputId : _max_event_id);
        }
        // 实际插入事件存储
        evt.emplace(e.GetId(), std::move(e));
        return _max_event_id;
    }

    void SecScoreDB::setEventErased(int id, bool isErased)
    {
        if (!this->evt.contains(id))
            throw std::runtime_error(std::format("Event ID {} not found.", id));
        auto it = evt.find(id);
        it->second.SetErased(isErased);
    }
}
