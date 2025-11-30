# SecScoreDB WebSocket 通信协议规范 v1.1

## 服务端启动说明

位于 `wrappers/websockets/main.cpp` 的服务端实现基于 `ixwebsocket`。构建完成后，会生成 `SecScoreDB-Websockets` 可执行文件。启动参数：

- `--port <number>`：监听的 WebSocket 端口，默认 `8765`。
- `--db <path>`：数据库根目录，默认当前工作目录下的 `data` 目录。

示例：

```powershell
cmake --build . --target SecScoreDB-Websockets
./SecScoreDB-Websockets --port 9000 --db ./data
```

服务端遵循本协议，所有请求和响应均采用 JSON。若请求解析失败或未通过验证，服务端会返回 `status="error"`，并根据情况设置 `code`（如 400、422、500 等）。

> **版本**: 1.1
> **变更**: 新增对字符串字段的模糊搜索支持 (`contains`, `starts_with`, `ends_with`)。

---

## 1. 协议概述

* **传输层**: WebSocket (WS/WSS)
* **编码格式**: JSON (UTF-8)
* **交互模式**: 全双工异步通信 (Request-Response)。
* **并发控制**: 客户端必须为每个请求生成唯一的序列号 (`seq`)。服务端保证在响应中原样返回该序列号，以便客户端将响应与请求进行匹配。

---

## 2. 通信信封结构 (Message Envelope)

所有消息（请求和响应）必须遵循以下统一结构。

### 2.1 客户端请求 (Client Request)

| 字段 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| **`seq`** | String | 是 | 请求唯一标识符 (如 UUID)。 |
| **`category`** | String | 是 | 目标资源分类: `"system"`, `"student"`, `"group"`, `"event"`. |
| **`action`** | String | 是 | 操作动作: `"define"`, `"create"`, `"query"`, `"update"`, `"delete"`. |
| **`payload`** | Object | 是 | 具体操作参数。 |

```json
{
    "seq": "req-abc-123",
    "category": "student",
    "action": "query",
    "payload": { ... }
}
```

### 2.2 服务端响应 (Server Response)

| 字段 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| **`seq`** | String | 是 | 原样回传请求的 `seq`。 |
| **`status`** | String | 是 | `"ok"` 或 `"error"`。 |
| **`code`** | Integer | 是 | 数字状态码 (详见第 6 节)。 |
| **`message`** | String | 否 | 可读的状态或错误描述信息。 |
| **`data`** | Object | 否 | 成功时的返回数据载荷。 |

```json
{
    "seq": "req-abc-123",
    "status": "ok",
    "code": 200,
    "data": { ... }
}
```

---

## 3. 系统管理操作 (`category: "system"`)

### 3.1 定义 Schema (Define)

初始化实体的动态字段定义。

* **Action**: `define`
* **Payload**:
  * `target`: `"student"` 或 `"group"`
  * `schema`: 字段名 -> 类型的映射表 (`"string"`, `"int"`, `"double"`).

**请求示例:**

```json
{
    "category": "system",
    "action": "define",
    "payload": {
        "target": "student",
        "schema": {
            "name": "string",
            "age": "int",
            "score": "double",
            "class": "string"
        }
    }
}
```

### 3.2 手动持久化 (Commit)

* **Action**: `commit`
* **Payload**: `{}`

用于显式触发持久化（相当于 `db.commit()`），方便客户端在批量写入后立即刷盘。

**请求示例**：

```json
{
    "category": "system",
    "action": "commit",
    "payload": {}
}
```

**响应示例**：

```json
{
    "status": "ok",
    "code": 200,
    "data": { "committed": true }
}
```

---

## 4. 核心资源操作 (`category: "student" | "group"`)

以下示例以 `student` 为例，`group` 同理。

### 4.1 批量创建 / 导入 (Batch Create)

支持一次性创建多个实体。处理 ID 自动生成与强制指定两种情况。

* **Action**: `create`
* **Payload**: `items` (数组)
  * `index`: 客户端临时索引，用于追踪。
  * `id`:
    * `null`: 请求服务端**自动生成** ID。
    * `Integer`: **强制使用**该 ID (通常用于数据导入/恢复)。
  * `data`: 动态字段的数据字典。

**请求示例:**

```json
{
    "category": "student",
    "action": "create",
    "payload": {
        "items": [
            {
                "index": 0,
                "id": null, 
                "data": { "name": "Alice", "age": 18, "class": "A-1" }
            },
            {
                "index": 1,
                "id": 999,
                "data": { "name": "Bob", "age": 20, "class": "B-2" }
            }
        ]
    }
}
```

**响应示例:**

```json
{
    "count": 2,
    "results": [
        { "index": 0, "success": true, "id": 1001 }, // 服务端分配的新 ID
        { "index": 1, "success": true, "id": 999 }    // 确认使用的 ID
    ]
}
```

### 4.2 逻辑查询 (Query) - 含模糊搜索

基于抽象语法树 (AST) 的复杂逻辑查询。

* **Action**: `query`
* **Payload**:
  * `logic`: 根逻辑节点。
    * `op`: 逻辑运算符 (`"AND"`, `"OR"`).
    * `rules`: 子规则或叶子条件数组。
  * `limit`: (可选) 最大返回记录数。

**叶子条件结构**:

* `field`: 字段名。
* `op`: 比较运算符 (见下表)。
* `val`: 比较值。

**运算符对照表**:

| 运算符 | 适用类型 | 说明 |
| :--- | :--- | :--- |
| `==`, `!=` | 所有类型 | 等于 / 不等于 |
| `>`, `<`, `>=`, `<=` | Int, Double | 数值大小比较 |
| **`contains`** | **String** | 字段包含该子串 |
| **`starts_with`** | **String** | 字段以该值开头 |
| **`ends_with`** | **String** | 字段以该值结尾 |

**请求示例:**

```json
{
    "category": "student",
    "action": "query",
    "payload": {
        "logic": {
            "op": "AND",
            "rules": [
                { "field": "age", "op": ">=", "val": 18 },
                {
                    "op": "OR",
                    "rules": [
                        { "field": "name", "op": "contains", "val": "Ali" },
                        { "field": "class", "op": "starts_with", "val": "2024" }
                    ]
                }
            ]
        }
    }
}
```

**响应示例:**

```json
{
    "items": [
        {
            "id": 1001,
            "data": { "name": "Alice", "age": 18, "class": "2024-A-1" }
        }
    ]
}
```

### 4.3 更新 (Update)

更新指定实体的字段。

* **Action**: `update`
* **Payload**:
  * `id`: 目标 ID。
  * `set`: 需要更新的字段映射表。

**请求示例:**

```json
{
    "category": "student",
    "action": "update",
    "payload": {
        "id": 1001,
        "set": {
            "score": 98.5,
            "active": 1
        }
    }
}
```

### 4.4 删除 (Delete)

根据 ID 删除实体。

* **Action**: `delete`
* **Payload**: `id` (Integer)。

**请求示例:**

```json
{
    "category": "student",
    "action": "delete",
    "payload": { "id": 1001 }
}
```

---

## 5. 事件操作 (`category: "event"`)

### 5.1 创建事件 (Create Event)

事件是追加型日志。`id` 必须为 `null` 以自动生成。

* **Action**: `create`
* **Payload**:
  * `id`: `null` (必须)。
  * `type`: `1` (Student) 或 `2` (Group)。
  * `ref_id`: 关联对象的 ID。
  * `desc`: 描述文本。
  * `val_prev`: 变更前数值 (double)。
  * `val_curr`: 变更后数值 (double)。

**请求示例:**

```json
{
    "category": "event",
    "action": "create",
    "payload": {
        "id": null,
        "type": 1,
        "ref_id": 1001,
        "desc": "Bonus points",
        "val_prev": 90.0,
        "val_curr": 95.0
    }
}
```

**响应示例:**

```json
{
    "id": 5001,
    "timestamp": 1710000000
}
```

### 5.2 标记擦除 (Soft Delete)

事件通常不会物理删除，而是标记为“已擦除”。

* **Action**: `update`
* **Payload**:
  * `id`: 事件 ID。
  * `erased`: Boolean (`true` 表示隐藏)。

**请求示例:**

```json
{
    "category": "event",
    "action": "update",
    "payload": {
        "id": 5001,
        "erased": true
    }
}
```

---

## 6. 状态码定义 (Status Codes)

| 代码 | 状态 | 说明 |
| :--- | :--- | :--- |
| **200** | OK | 请求处理成功。 |
| **400** | Bad Request | JSON 格式错误、缺少必填字段或不支持的 Action。 |
| **404** | Not Found | 目标资源 ID 不存在。 |
| **422** | Unprocessable | 逻辑错误 (如对 Int 字段使用 `contains`，或自动创建时 ID 不为 null)。 |
| **500** | Internal Error | 服务端内部异常或存储失败。 |
