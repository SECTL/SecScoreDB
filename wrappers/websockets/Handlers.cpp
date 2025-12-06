#include "Handlers.hpp"

#include <cmath>
#include <limits>
#include <chrono>

#include "JsonUtils.hpp"
#include "Permission.h"
#include "UserManager.h"

using nlohmann::json;

namespace ws
{
    template <typename WrapperFactory, typename CleanupFn, typename AllocFn>
    json handleCreate(const json& payload,
                      const SSDB::SchemaDef& schema,
                      WrapperFactory&& factory,
                      CleanupFn&& cleanup,
                      AllocFn&& allocator)
    {
        ensureSchemaReady(schema, "entity");
        if (!payload.contains("items") || !payload.at("items").is_array())
        {
            throw ApiError(400, "payload.items must be an array.");
        }

        const auto& items = payload.at("items");
        json results = json::array();
        size_t successCount = 0;
        for (const auto& item : items)
        {
            int index = item.value("index", 0);
            try
            {
                if (!item.contains("data"))
                {
                    throw ApiError(400, "Each item must include data.");
                }
                int id;
                if (!item.contains("id") || item.at("id").is_null())
                {
                    id = allocator();
                }
                else if (item.at("id").is_number_integer())
                {
                    id = item.at("id").get<int>();
                }
                else
                {
                    throw ApiError(422, "id must be null or integer.");
                }

                auto wrapper = factory(id);
                bool assigned = false;
                try
                {
                    assignDynamicFields(wrapper, item.at("data"), schema);
                    assigned = true;
                }
                catch (...)
                {
                    cleanup(id);
                    throw;
                }

                (void)assigned;
                results.push_back({{"index", index}, {"success", true}, {"id", id}});
                successCount++;
            }
            catch (const ApiError& err)
            {
                results.push_back({{"index", index}, {"success", false}, {"message", err.what()}});
            }
            catch (const std::exception& err)
            {
                results.push_back({{"index", index}, {"success", false}, {"message", err.what()}});
            }
        }
        return json{{"count", successCount}, {"results", results}};
    }

    template <typename MapType>
    json handleQuery(const json& payload,
                     const MapType& entities,
                     const SSDB::SchemaDef& schema)
    {
        ensureSchemaReady(schema, "entity");
        size_t limit = std::numeric_limits<size_t>::max();
        if (payload.contains("limit"))
        {
            if (!payload.at("limit").is_number_unsigned())
            {
                throw ApiError(400, "limit must be a non-negative integer.");
            }
            limit = payload.at("limit").get<size_t>();
            if (limit == 0)
            {
                limit = std::numeric_limits<size_t>::max();
            }
        }
        const json* logicNode = nullptr;
        if (payload.contains("logic"))
        {
            logicNode = &payload.at("logic");
        }

        json items = json::array();
        for (const auto& [id, entity] : entities)
        {
            auto data = materializeEntityData(entity, schema);
            bool matched = true;
            if (logicNode && !logicNode->is_null())
            {
                matched = evaluateLogicNode(data, *logicNode, schema);
            }
            if (matched)
            {
                items.push_back({{"id", id}, {"data", data}});
                if (items.size() >= limit)
                {
                    break;
                }
            }
        }
        return json{{"items", items}};
    }

    template <typename WrapperFetcher>
    json handleUpdate(const json& payload,
                      const SSDB::SchemaDef& schema,
                      WrapperFetcher&& fetcher)
    {
        ensureSchemaReady(schema, "entity");
        if (!payload.contains("id") || !payload.at("id").is_number_integer())
        {
            throw ApiError(400, "payload.id must be integer.");
        }
        if (!payload.contains("set"))
        {
            throw ApiError(400, "payload.set is required.");
        }
        int id = payload.at("id").get<int>();
        auto wrapper = fetcher(id);
        assignDynamicFields(wrapper, payload.at("set"), schema);
        return json{{"id", id}, {"updated", true}};
    }

    template <typename DeleteFn>
    json handleDelete(const json& payload,
                      std::string_view targetName,
                      DeleteFn&& deleter)
    {
        if (!payload.contains("id") || !payload.at("id").is_number_integer())
        {
            throw ApiError(400, "payload.id must be integer.");
        }
        int id = payload.at("id").get<int>();
        if (!deleter(id))
        {
            throw ApiError(404, std::string(targetName) + " id not found.");
        }
        return json{{"id", id}, {"deleted", true}};
    }

    SSDB::EventType parseEventType(int type)
    {
        if (type == 1) return SSDB::EventType::Student;
        if (type == 2) return SSDB::EventType::Group;
        throw ApiError(422, "event.type must be 1 (student) or 2 (group).");
    }

    json handleSystem(const std::string& actionRaw,
                      const json& payload,
                      RequestContext& ctx)
    {
        auto action = toLowerCopy(actionRaw);
        if (action == "commit")
        {
            std::scoped_lock lock(ctx.mutex);
            ctx.db.commit();
            return json{{"committed", true}};
        }
        if (action != "define")
        {
            throw ApiError(400, "Unsupported system action: " + actionRaw);
        }
        if (!payload.contains("target") || !payload.at("target").is_string())
        {
            throw ApiError(400, "payload.target must be string.");
        }
        if (!payload.contains("schema"))
        {
            throw ApiError(400, "payload.schema is required.");
        }

        auto target = toLowerCopy(payload.at("target").get<std::string>());
        auto schema = parseSchema(payload.at("schema"));
        std::scoped_lock lock(ctx.mutex);
        if (target == "student")
        {
            ctx.db.initStudentSchema(schema);
        }
        else if (target == "group")
        {
            ctx.db.initGroupSchema(schema);
        }
        else
        {
            throw ApiError(400, "target must be 'student' or 'group'.");
        }
        return json{{"target", target}, {"fields", schema.size()}};
    }

    json handleStudent(const std::string& actionRaw,
                       const json& payload,
                       RequestContext& ctx)
    {
        std::scoped_lock lock(ctx.mutex);
        const auto& schema = ctx.db.studentSchema();
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            auto result = handleCreate(payload, schema,
                [&](int id) { return ctx.db.createStudent(id); },
                [&](int id) { ctx.db.deleteStudent(id); },
                [&]() { return ctx.db.allocateStudentId(); });
            if (result.at("count").get<size_t>() > 0)
            {
                ctx.db.commit();
            }
            return result;
        }
        if (action == "query")
        {
            return handleQuery(payload, ctx.db.students(), schema);
        }
        if (action == "update")
        {
            auto result = handleUpdate(payload, schema, [&](int id) { return ctx.db.getStudent(id); });
            ctx.db.commit();
            return result;
        }
        if (action == "delete")
        {
            auto result = handleDelete(payload, "student", [&](int id) { return ctx.db.deleteStudent(id); });
            ctx.db.commit();
            return result;
        }
        throw ApiError(400, "Unsupported student action: " + actionRaw);
    }

    json handleGroup(const std::string& actionRaw,
                     const json& payload,
                     RequestContext& ctx)
    {
        std::scoped_lock lock(ctx.mutex);
        const auto& schema = ctx.db.groupSchema();
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            auto result = handleCreate(payload, schema,
                [&](int id) { return ctx.db.createGroup(id); },
                [&](int id) { ctx.db.deleteGroup(id); },
                [&]() { return ctx.db.allocateGroupId(); });
            if (result.at("count").get<size_t>() > 0)
            {
                ctx.db.commit();
            }
            return result;
        }
        if (action == "query")
        {
            return handleQuery(payload, ctx.db.groups(), schema);
        }
        if (action == "update")
        {
            auto result = handleUpdate(payload, schema, [&](int id) { return ctx.db.getGroup(id); });
            ctx.db.commit();
            return result;
        }
        if (action == "delete")
        {
            auto result = handleDelete(payload, "group", [&](int id) { return ctx.db.deleteGroup(id); });
            ctx.db.commit();
            return result;
        }
        throw ApiError(400, "Unsupported group action: " + actionRaw);
    }

    json handleEvent(const std::string& actionRaw,
                     const json& payload,
                     RequestContext& ctx)
    {
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            if (!payload.contains("id") || !payload.at("id").is_null())
            {
                throw ApiError(422, "event.id must be null for auto generation.");
            }
            if (!payload.contains("type") || !payload.at("type").is_number_integer())
            {
                throw ApiError(400, "event.type must be integer.");
            }
            if (!payload.contains("ref_id") || !payload.at("ref_id").is_number_integer())
            {
                throw ApiError(400, "event.ref_id must be integer.");
            }
            if (!payload.contains("desc") || !payload.at("desc").is_string())
            {
                throw ApiError(400, "event.desc must be string.");
            }
            if (!payload.contains("val_prev") || !payload.contains("val_curr"))
            {
                throw ApiError(400, "event.val_prev and event.val_curr are required.");
            }
            double prev = requireNumber(payload.at("val_prev"), "val_prev");
            double curr = requireNumber(payload.at("val_curr"), "val_curr");

            SSDB::Event evt;
            evt.SetId(SSDB::INVALID_ID);
            evt.SetEventType(parseEventType(payload.at("type").get<int>()));
            evt.SetOperatingObject(payload.at("ref_id").get<int>());
            evt.SetReason(payload.at("desc").get<std::string>());
            evt.SetDeltaScore(static_cast<int>(std::llround(curr - prev)));

            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(evt.GetEventTime().time_since_epoch()).count();

            std::scoped_lock lock(ctx.mutex);
            int id = ctx.db.addEvent(evt);
            ctx.db.commit();
            return json{{"id", id}, {"timestamp", timestamp}};
        }
        if (action == "update")
        {
            if (!payload.contains("id") || !payload.at("id").is_number_integer())
            {
                throw ApiError(400, "event.id must be integer.");
            }
            if (!payload.contains("erased") || !payload.at("erased").is_boolean())
            {
                throw ApiError(400, "event.erased must be boolean.");
            }
            std::scoped_lock lock(ctx.mutex);
            int id = payload.at("id").get<int>();
            ctx.db.setEventErased(id, payload.at("erased").get<bool>());
            ctx.db.commit();
            return json{{"id", id}, {"erased", payload.at("erased").get<bool>()}};
        }
        throw ApiError(400, "Unsupported event action: " + actionRaw);
    }

    // 辅助函数：将 Permission 转换为 JSON 字符串
    std::string permissionToJsonString(SSDB::Permission perm)
    {
        using SSDB::Permission;
        if (perm == Permission::ROOT) return "root";
        if (perm == Permission::NONE) return "none";

        std::string result;
        if (SSDB::hasPermission(perm, Permission::READ))
        {
            result += "read";
        }
        if (SSDB::hasPermission(perm, Permission::WRITE))
        {
            if (!result.empty()) result += ",";
            result += "write";
        }
        if (SSDB::hasPermission(perm, Permission::DELETE))
        {
            if (!result.empty()) result += ",";
            result += "delete";
        }
        return result.empty() ? "none" : result;
    }

    // 辅助函数：从 JSON 字符串解析 Permission
    SSDB::Permission parsePermissionFromJson(const json& val)
    {
        using SSDB::Permission;

        if (val.is_string())
        {
            std::string str = toLowerCopy(val.get<std::string>());
            if (str == "root") return Permission::ROOT;
            if (str == "none") return Permission::NONE;
            if (str == "read") return Permission::READ;
            if (str == "write") return Permission::WRITE;
            if (str == "delete") return Permission::DELETE;
            if (str == "read,write" || str == "write,read") return Permission::READ | Permission::WRITE;
            if (str == "read,delete" || str == "delete,read") return Permission::READ | Permission::DELETE;
            if (str == "write,delete" || str == "delete,write") return Permission::WRITE | Permission::DELETE;

            // 解析逗号分隔的权限
            Permission result = Permission::NONE;
            if (str.find("read") != std::string::npos) result = result | Permission::READ;
            if (str.find("write") != std::string::npos) result = result | Permission::WRITE;
            if (str.find("delete") != std::string::npos) result = result | Permission::DELETE;
            return result;
        }
        else if (val.is_array())
        {
            Permission result = Permission::NONE;
            for (const auto& p : val)
            {
                if (p.is_string())
                {
                    std::string s = toLowerCopy(p.get<std::string>());
                    if (s == "read") result = result | Permission::READ;
                    else if (s == "write") result = result | Permission::WRITE;
                    else if (s == "delete") result = result | Permission::DELETE;
                    else if (s == "root") return Permission::ROOT;
                }
            }
            return result;
        }

        throw ApiError(422, "permission must be a string or array of strings.");
    }

    json handleUser(const std::string& actionRaw,
                    const json& payload,
                    RequestContext& ctx)
    {
        auto action = toLowerCopy(actionRaw);
        auto& userMgr = ctx.db.userManager();

        // 登录操作
        if (action == "login")
        {
            if (!payload.contains("username") || !payload.at("username").is_string())
            {
                throw ApiError(400, "payload.username must be string.");
            }
            if (!payload.contains("password") || !payload.at("password").is_string())
            {
                throw ApiError(400, "payload.password must be string.");
            }

            std::scoped_lock lock(ctx.mutex);
            std::string username = payload.at("username").get<std::string>();
            std::string password = payload.at("password").get<std::string>();

            // 使用 UserManager 验证密码，但登录状态保存在 ctx 中
            auto userOpt = userMgr.findUserByUsername(username);
            if (userOpt)
            {
                const auto& user = userOpt->get();
                if (user.IsActive() && userMgr.verifyPassword(user.GetId(), password))
                {
                    ctx.login(user.GetId());
                    return json{
                        {"success", true},
                        {"user", {
                            {"id", user.GetId()},
                            {"username", user.GetUsername()},
                            {"permission", permissionToJsonString(user.GetPermission())}
                        }}
                    };
                }
            }
            throw ApiError(401, "Invalid username or password.");
        }

        // 登出操作
        if (action == "logout")
        {
            ctx.logout();
            return json{{"success", true}};
        }

        // 获取当前用户信息
        if (action == "current")
        {
            if (!ctx.isLoggedIn())
            {
                return json{{"logged_in", false}};
            }

            std::scoped_lock lock(ctx.mutex);
            auto userOpt = userMgr.findUserById(*ctx.currentUserId);
            if (userOpt)
            {
                const auto& user = userOpt->get();
                return json{
                    {"logged_in", true},
                    {"user", {
                        {"id", user.GetId()},
                        {"username", user.GetUsername()},
                        {"permission", permissionToJsonString(user.GetPermission())},
                        {"active", user.IsActive()}
                    }}
                };
            }
            // 用户已被删除
            ctx.logout();
            return json{{"logged_in", false}};
        }

        // 以下操作需要登录
        if (!ctx.isLoggedIn())
        {
            throw ApiError(401, "Login required.");
        }

        std::scoped_lock lock(ctx.mutex);

        // 获取当前用户权限
        auto currentUserOpt = userMgr.findUserById(*ctx.currentUserId);
        if (!currentUserOpt)
        {
            ctx.logout();
            throw ApiError(401, "Session expired. Please login again.");
        }
        const auto& currentUser = currentUserOpt->get();
        bool isRoot = SSDB::hasPermission(currentUser.GetPermission(), SSDB::Permission::ROOT);

        // 创建用户（需要 root 权限）
        if (action == "create")
        {
            if (!isRoot)
            {
                throw ApiError(403, "Only root user can create new users.");
            }

            if (!payload.contains("username") || !payload.at("username").is_string())
            {
                throw ApiError(400, "payload.username must be string.");
            }
            if (!payload.contains("password") || !payload.at("password").is_string())
            {
                throw ApiError(400, "payload.password must be string.");
            }

            std::string username = payload.at("username").get<std::string>();
            std::string password = payload.at("password").get<std::string>();

            SSDB::Permission perm = SSDB::Permission::READ;
            if (payload.contains("permission"))
            {
                perm = parsePermissionFromJson(payload.at("permission"));
            }

            try
            {
                // 临时登录 root 到 UserManager 来创建用户
                userMgr.login(currentUser.GetUsername(), "");  // 已验证，跳过密码检查
                // 直接创建，因为我们已经验证了 isRoot
                int newId = userMgr.getNextUserId();
                SSDB::User newUser(newId, username, userMgr.hashPassword(password), perm);
                userMgr.addUser(newUser);
                userMgr.commit();
                userMgr.logout();

                return json{
                    {"success", true},
                    {"user", {
                        {"id", newId},
                        {"username", username},
                        {"permission", permissionToJsonString(perm)}
                    }}
                };
            }
            catch (const std::runtime_error& e)
            {
                throw ApiError(409, e.what());
            }
        }

        // 删除用户（需要 root 权限）
        if (action == "delete")
        {
            if (!isRoot)
            {
                throw ApiError(403, "Only root user can delete users.");
            }

            int userId = -1;
            std::string username;

            if (payload.contains("id") && payload.at("id").is_number_integer())
            {
                userId = payload.at("id").get<int>();
            }
            else if (payload.contains("username") && payload.at("username").is_string())
            {
                username = payload.at("username").get<std::string>();
                auto userOpt = userMgr.findUserByUsername(username);
                if (userOpt)
                {
                    userId = userOpt->get().GetId();
                }
            }
            else
            {
                throw ApiError(400, "payload.id (integer) or payload.username (string) is required.");
            }

            if (userId < 0)
            {
                throw ApiError(404, "User not found.");
            }

            // 不能删除自己
            if (userId == *ctx.currentUserId)
            {
                throw ApiError(400, "Cannot delete yourself.");
            }

            try
            {
                bool deleted = userMgr.removeUser(userId);
                if (!deleted)
                {
                    throw ApiError(404, "User not found.");
                }
                userMgr.commit();
                return json{{"success", true}, {"deleted", true}};
            }
            catch (const std::runtime_error& e)
            {
                throw ApiError(400, e.what());
            }
        }

        // 更新用户（修改权限、密码、状态）
        if (action == "update")
        {
            if (!payload.contains("id") || !payload.at("id").is_number_integer())
            {
                throw ApiError(400, "payload.id must be integer.");
            }
            int userId = payload.at("id").get<int>();

            try
            {
                // 修改权限（需要 root）
                if (payload.contains("permission"))
                {
                    if (!isRoot)
                    {
                        throw ApiError(403, "Only root user can modify permissions.");
                    }
                    auto perm = parsePermissionFromJson(payload.at("permission"));
                    auto userOpt = userMgr.findUserById(userId);
                    if (!userOpt)
                    {
                        throw ApiError(404, "User not found.");
                    }
                    // 直接修改用户权限
                    userMgr.updateUserPermission(userId, perm);
                }

                // 修改密码
                if (payload.contains("new_password") && payload.at("new_password").is_string())
                {
                    std::string newPassword = payload.at("new_password").get<std::string>();

                    // 修改自己的密码需要旧密码
                    if (userId == *ctx.currentUserId)
                    {
                        if (!payload.contains("old_password") || !payload.at("old_password").is_string())
                        {
                            throw ApiError(400, "old_password is required to change your own password.");
                        }
                        std::string oldPassword = payload.at("old_password").get<std::string>();
                        if (!userMgr.verifyPassword(userId, oldPassword))
                        {
                            throw ApiError(401, "Old password is incorrect.");
                        }
                    }
                    else if (!isRoot)
                    {
                        throw ApiError(403, "Only root user can change other users' passwords.");
                    }

                    userMgr.updateUserPassword(userId, newPassword);
                }

                // 修改激活状态（需要 root）
                if (payload.contains("active") && payload.at("active").is_boolean())
                {
                    if (!isRoot)
                    {
                        throw ApiError(403, "Only root user can enable/disable users.");
                    }
                    if (userId == *ctx.currentUserId && !payload.at("active").get<bool>())
                    {
                        throw ApiError(400, "Cannot disable yourself.");
                    }
                    userMgr.updateUserActive(userId, payload.at("active").get<bool>());
                }

                userMgr.commit();
                return json{{"success", true}, {"id", userId}};
            }
            catch (const std::runtime_error& e)
            {
                throw ApiError(400, e.what());
            }
        }

        // 查询用户列表
        if (action == "query" || action == "list")
        {

            json users = json::array();
            for (const auto& [id, user] : userMgr.allUsers())
            {
                users.push_back({
                    {"id", user.GetId()},
                    {"username", user.GetUsername()},
                    {"permission", permissionToJsonString(user.GetPermission())},
                    {"active", user.IsActive()}
                });
            }
            return json{{"users", users}};
        }

        throw ApiError(400, "Unsupported user action: " + actionRaw);
    }
 }
