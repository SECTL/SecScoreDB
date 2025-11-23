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
        DataBaseFile          stu_db;
        DataBaseFile          grp_db;
        DataBaseFile          evt_db;
        std::unordered_map<int,Student> stu;
        std::unordered_map<int,Event>   evt;
        std::unordered_map<int,Group>   grp;

        //schema
        SchemaDef _stu_schema;
        SchemaDef _grp_schema;

    public:
        SecScoreDB(const std::filesystem::path& path);
        ~SecScoreDB();

        // init schema for grp & stu

        void initStudentSchema(const SchemaDef& schema)
        {
            this->_stu_schema=schema;
        }
        void initGroupSchema(const SchemaDef& schema)
        {
            this->_grp_schema=schema;
        }

        // 增
        DynamicWrapper<Student> createStudent(int id);
        DynamicWrapper<Group> createGroup(int id);

        DynamicWrapper<Student> addStudent(Student s);
        DynamicWrapper<Group> addGroup(Group g);
        DynamicWrapper<Student> addStudent(DynamicWrapper<Student> s);
        DynamicWrapper<Group> addGroup(DynamicWrapper<Group> g);

        //查
        DynamicWrapper<Student> getStudent(int id);
        DynamicWrapper<Group> getGroup(int id);

        template<typename Predicate>
        std::vector<DynamicWrapper<Student>> getStudent(Predicate&& pred);
        template<typename Predicate>
        std::vector<DynamicWrapper<Student>> getGroup(Predicate&& pred);

        //删
        bool deleteStudent(int id);
        bool deleteGroup(int id);

        template<typename Predicate>
        size_t deleteStudent(Predicate&& pred);
        template<typename Predicate>
        size_t deleteGroup(Predicate&& pred);

        //改：外面直接操作

        // 数据库事务相关

        void commit();
    };
}
