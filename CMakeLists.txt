cmake_minimum_required(VERSION 3.26)  # VS 2026需要较新的CMake版本
project(SecScoreDB)

# 设置C++23标准 - VS 2026 Insiders完全支持
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展，使用标准C++

# 查找依赖包
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)

# 添加静态库 - 只包含源文件，不包含头文件
add_library(SecScoreDB STATIC
        src/FileHelper.cpp
        src/SecScoreDB.cpp
        src/Schema.hpp
        src/DynamicFields.hpp
)

# 设置包含目录 - 确保头文件可以被正确找到
target_include_directories(SecScoreDB PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖库到我们的静态库
target_link_libraries(SecScoreDB PRIVATE
        nlohmann_json::nlohmann_json
        cereal::cereal
)

# 如果需要链接到可执行文件，应该先定义可执行文件
# 例如：
# add_executable(main main.cpp)
# target_link_libraries(main PRIVATE Secscoredb_cmake)