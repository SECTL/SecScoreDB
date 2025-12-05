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

# 排除 websocket_client_test.cpp，它会单独编译
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*websocket_client_test\\.cpp$")

if (TEST_SOURCES)
    add_executable(SecScoreDBTest ${TEST_SOURCES})
    # 链接到我们刚刚的静态库
    target_link_libraries(SecScoreDBTest PRIVATE SecScoreDB)
    # 由于静态库已经 PUBLIC 暴露头文件，这里理论上不需要再次 include
    # 但如果以后 tests 里需要直接访问 src 下的未公开部分，可加：
    target_include_directories(SecScoreDBTest PRIVATE ${PROJECT_SOURCE_DIR}/src)
endif()

# -------------------------------------------------------------
# WebSocket 客户端测试程序
# 用于测试 SecScoreDB-Websockets 服务端功能
# 使用方法：
#   1. 先启动服务端：./SecScoreDB-Websockets --port 8765 --db ./testdata_ws
#   2. 再运行测试：./SecScoreDBWebsocketTest
#
# 注意: 如果遇到链接错误 (如 _RTC_CheckStackVars, memcpy 等未定义)，
# 这通常是 vcpkg 库与当前编译器/运行时设置不兼容导致的。
# 解决方法:
#   1. 删除 cmake-build-* 目录和 vcpkg_installed 目录
#   2. 重新运行 cmake 配置
#   3. 确保 vcpkg triplet 匹配 (x64-windows 或 x64-windows-static)
# -------------------------------------------------------------
if (EXISTS "${PROJECT_SOURCE_DIR}/tests/websocket_client_test.cpp")
    add_executable(SecScoreDBWebsocketTest
        tests/websocket_client_test.cpp
    )
    target_link_libraries(SecScoreDBWebsocketTest PRIVATE
        SecScoreDB
        ixwebsocket::ixwebsocket
        bcrypt  # Windows: mbedtls 依赖
    )
    target_include_directories(SecScoreDBWebsocketTest PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
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
