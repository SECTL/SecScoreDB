cmake_minimum_required(VERSION 3.26)  # VS 2026需要较新的CMake版本
project(SecScoreDB)

# 设置C++23标准 - VS 2026 Insiders完全支持
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展，使用标准C++

if (MSVC)
    add_compile_options(/utf-8)
    add_compile_options(/wd4819) # 可选：如果转换后还报错，可以屏蔽这个特定警告
endif()

# 查找依赖包
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)

# 添加静态库 - 只包含源文件，不包含头文件
add_library(SecScoreDB STATIC
        src/FileHelper.cpp
        src/SecScoreDB.cpp
        src/Schema.hpp
        src/DynamicFields.hpp
)

# 设置包含目录 - 确保头文件可以被正确找到
# PUBLIC 传播给链接该库的目标
# INSTALL_INTERFACE 以后安装可用
target_include_directories(SecScoreDB PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖库到我们的静态库
target_link_libraries(SecScoreDB PUBLIC
        nlohmann_json::nlohmann_json
        cereal::cereal
)

# -------------------------------------------------------------
# 测试/示例可执行文件: 自动收集 tests 目录下的所有 .cpp
# -------------------------------------------------------------
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/tests/*.cpp)

if (TEST_SOURCES)
    add_executable(SecScoreDBTest ${TEST_SOURCES}
            tests/main.cpp)
    # 链接到我们刚刚的静态库
    target_link_libraries(SecScoreDBTest PRIVATE SecScoreDB)
    # 由于静态库已经 PUBLIC 暴露头文件，这里理论上不需要再次 include
    # 但如果以后 tests 里需要直接访问 src 下的未公开部分，可加：
    target_include_directories(SecScoreDBTest PRIVATE ${PROJECT_SOURCE_DIR}/src)
endif()

# 如果需要链接到可执行文件，应该先定义可执行文件
# 例如：
# add_executable(main main.cpp)
# target_link_libraries(main PRIVATE Secscoredb_cmake)

# -------------------------------------------------------------
# Wrappers / example executables
# -------------------------------------------------------------
# Add a WebSockets wrapper executable that links to our SecScoreDB static library
if (EXISTS "${PROJECT_SOURCE_DIR}/wrappers/websockets/main.cpp")
    add_executable(SecScoreDB-Websockets
        wrappers/websockets/main.cpp
        wrappers/websockets/Protocol.cpp
        wrappers/websockets/Handlers.cpp
        wrappers/websockets/JsonUtils.cpp
        wrappers/websockets/Errors.cpp
    )
    # Link against the library we defined earlier

    target_link_libraries(SecScoreDB-Websockets PRIVATE SecScoreDB)
    # mbedtls (mbedcrypto) may call BCryptGenRandom on Windows; link bcrypt.lib to resolve it
    target_link_libraries(SecScoreDB-Websockets PRIVATE ixwebsocket::ixwebsocket bcrypt)
    # Ensure the wrapper can find project headers and its own headers
    target_include_directories(SecScoreDB-Websockets PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/wrappers/websockets
    )
endif()
