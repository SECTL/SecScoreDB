#pragma once
#include "FileHelper.h"
#include <string>
#include "Student.h"
#include "Event.h"
#include "Group.h"
#include <unordered_map>

namespace SSDB
{
    class SecScoreDB
    {
    private:
        DataBaseFile          _db;
        std::unordered_map<int,Student> stu;
        std::unordered_map<int,Event>   evt;
        std::unordered_map<int,Group>   grp;

        //用于还原相关
        bool dirty=false;
        std::unordered_map<int,Student> org_stu;
        std::unordered_map<int,Event>   org_evt;
        std::unordered_map<int,Group>   org_grp;

    };
}