#include "SecScoreDB.h"

namespace SSDB
{
    SecScoreDB::SecScoreDB(const std::filesystem::path& path)
        : stu_db(path / "stu.db")
        , grp_db(path / "grp.db")
        , evt_db(path / "evt.db")
    {
        //首先检查，给出的应该是目录，下面将会创建（如果不存在）: stu.db,evt.db,grp.db;
        if (!fs::exists(path))
            throw std::runtime_error("The path does not exist");
        else if (!fs::is_directory(path))
            throw std::runtime_error("Must be path");
    }

    SecScoreDB::~SecScoreDB()
    {

    }

    

}