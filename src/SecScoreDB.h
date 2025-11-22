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

        //用于还原相关
        bool dirty=false;
        std::unordered_map<int,Student> org_stu;
        std::unordered_map<int,Event>   org_evt;
        std::unordered_map<int,Group>   org_grp;

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


        // 获取 Student / Group 对象
        DynamicWrapper<Student> getStudent(int id);
        DynamicWrapper<Group> getGroup(int id);
    };
}
