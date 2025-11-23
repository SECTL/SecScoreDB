# SecScoreDB - 一个为 SecScore 而设计的 "数据库"

## 简介

SecScoreDB 是一个专为 SecScore 项目设计的轻量级数据库系统。它提供了基于文件的持久化存储，支持学生、组和事件等实体的管理，具备动态字段和类型安全的特性。

该数据库系统使用 C++23 编写，利用 cereal 库进行序列化，支持现代 C++ 特性，如概念（concepts）、模板元编程和范围（ranges）等。

## 特性

- **轻量级文件存储**：使用二进制文件存储数据，高效且跨平台
- **动态字段支持**：通过 Schema 定义和 DynamicWrapper 实现动态字段管理
- **类型安全**：在编译时和运行时进行类型检查，确保数据一致性
- **现代化 C++**：充分利用 C++20/23 特性，包括 concepts、模板元编程等
- **自动序列化**：使用 cereal 库实现数据的自动序列化和反序列化
- **Lambda 查询**：支持使用 Lambda 表达式进行灵活的数据查询和过滤

## 核心组件

### 主要类

1. **SecScoreDB** - 主数据库类，提供所有数据操作接口
2. **Student** - 学生实体，支持动态元数据
3. **Group** - 组实体，管理学生分组
4. **Event** - 事件实体，记录操作历史
5. **DynamicWrapper** - 动态字段包装器，提供类型安全的字段访问
6. **DataBaseFile** - 文件操作辅助类，处理数据持久化

### 动态字段系统

SecScoreDB 支持通过 Schema 定义动态字段，目前支持以下类型：
- Int（整数）
- Double（浮点数）
- String（字符串）

通过 DynamicWrapper，可以像访问普通对象属性一样访问这些动态字段，同时保持类型安全。

## 安装与构建

### 环境要求

- C++23 兼容编译器（推荐 Visual Studio 2022 (MSVC v143) 或 GCC 13+）
- CMake 3.26 或更高版本
- vcpkg 包管理器

### 依赖项

- nlohmann/json
- cereal

### 构建步骤

```bash
# 克隆项目
git clone <repository-url>
cd SecScoreDB

# 使用 vcpkg 安装依赖
vcpkg install

# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake

# 构建项目
cmake --build .
```

## 使用示例

见 tests/main.cpp

## 许可证

本项目采用 GNU Lesser General Public License v2.1 许可证。详见 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request 来改进这个项目。