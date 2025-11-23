#pragma once
#include "FileHelper.h"
#include <string>
#include "Student.h"
#include "Event.h"
#include "Group.h"
#include <unordered_map>
#include <filesystem>

#include "DynamicFields.hpp"

namespace SSDB
{
    class SecScoreDB
    {
    private:
        DataBaseFile stu_db;
        DataBaseFile grp_db;
        DataBaseFile evt_db;
        std::unordered_map<int, Student> stu;
        std::unordered_map<int, Event> evt;
        std::unordered_map<int, Group> grp;

        //schema
        SchemaDef _stu_schema;
        SchemaDef _grp_schema;

        void assertStudentSchema() const
        {
            if (_stu_schema.empty())
                throw std::runtime_error("Operation failed: Student Schema is not initialized.");
        }

        void assertGroupSchema() const
        {
            if (_grp_schema.empty())
                throw std::runtime_error("Operation failed: Group Schema is not initialized.");
        }

    public:
        SecScoreDB(const std::filesystem::path& path);
        ~SecScoreDB();

        // init schema for grp & stu

        void initStudentSchema(const SchemaDef& schema)
        {
            this->_stu_schema = schema;
        }

        void initGroupSchema(const SchemaDef& schema)
        {
            this->_grp_schema = schema;
        }

        // 增
        DynamicWrapper<Student> createStudent(int id);
        DynamicWrapper<Group> createGroup(int id);

        DynamicWrapper<Student> addStudent(Student s);
        DynamicWrapper<Group> addGroup(Group g);
        DynamicWrapper<Student> addStudent(const DynamicWrapper<Student>& s);
        DynamicWrapper<Group> addGroup(const DynamicWrapper<Group>& g);

        //查
        DynamicWrapper<Student> getStudent(int id);
        DynamicWrapper<Group> getGroup(int id);

        template <typename Predicate>
        std::vector<DynamicWrapper<Student>> getStudent(Predicate&& pred)
        {
            std::vector<DynamicWrapper<Student>> results;

            // 遍历内存中的所有学生
            for (auto& [id, entity] : stu)
            {
                // 1. 创建一个临时的 Wrapper 用于检查条件
                DynamicWrapper<Student> wrapper(entity, _stu_schema);

                try
                {
                    // 2. 执行用户传入的 Lambda
                    // 如果 Lambda 返回 true，则匹配成功
                    if (pred(wrapper))
                    {
                        // 3. 将匹配的对象加入结果集
                        // 注意：我们重新构造一个 Wrapper 放入 vector，因为 Wrapper 是引用视图，拷贝开销极小
                        results.push_back(DynamicWrapper<Student>(entity, _stu_schema));
                    }
                }
                catch (...)
                {
                    // 4. 容错处理
                    // 如果 Lambda 中访问了不存在的字段或类型错误导致抛出异常，
                    // 我们认为这条记录不匹配，直接跳过 (continue)
                    continue;
                }
            }
            return results;
        }

        template <typename Predicate>
        std::vector<DynamicWrapper<Group>> getGroup(Predicate&& pred)
        {
            std::vector<DynamicWrapper<Group>> results;

            for (auto& [id, entity] : grp)
            {
                DynamicWrapper<Group> wrapper(entity, _grp_schema);

                try
                {
                    if (pred(wrapper))
                    {
                        results.push_back(DynamicWrapper<Group>(entity, _grp_schema));
                    }
                }
                catch (...)
                {
                    continue;
                }
            }
            return results;
        }

        //删
        bool deleteStudent(int id);
        bool deleteGroup(int id);

        template <typename Predicate>
        size_t deleteStudent(Predicate&& pred)
        {
            // C++20 std::erase_if
            // 遍历 map，如果 pred 返回 true 则删除该元素
            return std::erase_if(stu, [&](auto& pair)
            {
                // pair.second 是 Student 对象
                // 我们需要 const_cast，因为 DynamicWrapper 通常接受非 const 引用
                // 但我们在 predicate 里应该只读，不改。
                Student& entity = const_cast<Student&>(pair.second);

                DynamicWrapper<Student> wrapper(entity, _stu_schema);
                try
                {
                    return pred(wrapper);
                }
                catch (...)
                {
                    return false; // 出错不删
                }
            });
        }

        template <typename Predicate>
        size_t deleteGroup(Predicate&& pred)
        {
            return std::erase_if(grp, [&](auto& pair)
            {
                                Group& entity = pair.second;
                DynamicWrapper<Group> wrapper(entity, _grp_schema);
                try
                {
                    return pred(wrapper);
                }
                catch (...)
                {
                    return false;
                }
            });
        }

        //改：外面直接操作

        // 数据库事务相关

        void commit();
    };
}
