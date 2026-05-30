# Core.State / AppState

本文档说明 `AppState` 的写法、动机和 MSVC 模块下的常见坑，供后续维护（含 AI）快速对齐上下文。

## 结构

| 文件 | 职责 |
|------|------|
| `app_state.ixx` | 导出 `Core.State`：`AppState` 布局 + **各子系统 state 的前向声明**（不 `import` 任何 `X.State`） |
| `app_state.cpp` | `AppState` 构造/析构：`import` 各 `X.State` 完整类型，供 `make_unique` 使用 |

子模块仍保留各自的 `state.ixx`（完整定义）。**不要**把子 state 拆成「State 仅前向声明 + Types 放定义」作为常规做法；`AppState` 自身也**不要**拆 `Core.State` / `Core.State.Types`。

## 根本原因：避免雪崩重编

`Core.State` 被大量模块 `import`，处在依赖图中心。

若在 `app_state.ixx` 里 **直接 `import` 各子系统的 `X.State`**（带入完整 struct 布局），则任意子 state 增删字段都会：

1. 重编该 `X.State`
2. **连带重编 `Core.State`**
3. **再连带重编所有 `import Core.State` 的模块**（RPC、Initializer、各 Feature、UI…）

一次改 Gallery 字段，整树重编，即典型的 **雪崩重编**。这是采用前向声明的首要动机，不是可选优化。

## 因此：`app_state.ixx` 用前向声明

- `app_state.ixx`：只写 `export struct XxxState;` + `std::unique_ptr<XxxState>` 成员，**不 import** 任何 `X.State`
- `app_state.cpp`：单独 `import` 各 `X.State`，负责 `make_unique` / 析构

子 state 布局变更时，理想情况仅重编该子模块 + `app_state.cpp`，**`Core.State` 接口模块本身不应因字段变化而重编**。

## 公开 API 约定（前向声明带来的第二约束）

跨模块 **export** 的函数，参数应优先使用：

```cpp
Core::State::AppState&        // 或 const AppState&
```

**避免**在 `.ixx` 接口里暴露 `X::State::FooState&`，并由调用方写 `*state.foo` 传入。

原因：MSVC 会把 `app_state.ixx` 里的前向声明与 `X.State` 模块里的完整定义当成**不同的类型实体**，导致 **LNK2019**（声明要 `FooState[Core.State]&`，实现导出 `FooState[X.State]&`）。`I18n`、`Events`、`Commands` 等已踩过此坑。

已对齐的 Core 示例：`Async`、`WorkerPool`、`I18n`、`DialogService`（start/stop）、`Events`（send/post/subscribe）、`Database`（initialize/shutdown）、`Commands`（invoke_command / get_command / is_toggle_on）等。

## 实现侧访问子 state

`AppState` 在 `app_state.cpp` 构造时即 `make_unique` 全部子 state；**应用完成初始化后的正常运行路径下，各 `state.xxx` 成员均非空，不必对每个子 state 做 null 检查**。

| 场景 | 做法 |
|------|------|
| 模块 **.cpp** 实现 | `import Core.State` + 对应 `X.State`，通过 `state.xxx->field` 直接访问；只有调用指针 API 时才用 `.get()` |
| **.ixx** 里的 export 模板会调用到的 helper | 优先让模板只做类型相关工作，把访问子 state、调度线程、持锁等逻辑下沉到 `.cpp` 的非模板函数 |
| **.ixx** 内联模板若必须碰子 state 成员 | 先重新评估边界；确实无法下沉时，helper 才和模板放在同一个 `.ixx`，并通常标 `inline` |
| 只读访问子模块聚合数据（如命令表） | 在所属模块提供 `foo(AppState&)` 业务能力（如 `invoke_command`），勿让外部 `state.commands->registry` |

`auto& foo = *state.foo` 仅在同一函数内重复访问很多字段、且希望缩短写法时**可选**使用，不是规范要求。

范例：`Features.Recording.UseCase`（`usecase.cpp`）中 `state.recording->status`、`state.settings->raw`、`state.i18n->texts` 均为直接访问，无子 state 空检查。

`app_state.cpp` 是唯一集中 `make_unique` 的地方；新增子 state 时在此 `import` 对应 `X.State`。

## 新增子 state 检查清单

1. 在 `app_state.ixx` 对应命名空间增加 `export struct XxxState;`
2. 在 `AppState` 中增加 `std::unique_ptr<...::XxxState> xxx;`
3. 在 `app_state.cpp` `import` 该 `X.State`，并在构造函数中 `make_unique`
4. 该 feature 的 **export API** 用 `AppState&`，实现里再取 `state.xxx`
5. 代码风格：`namespace` + `export struct`，**不用** `export namespace`

## export 模板与非模板执行器（以 Core.Database 为例）

### 和「API 用 `AppState&`」解决的不是同一类问题

- **`AppState&`**：避免调用方写 `*state.database` / `*state.recording`，参数类型在 `Core.State` 与 `X.State` 之间对不上 → LNK2019。
- **非模板执行器**：避免 **export 模板** 在 Gallery / Migration 等 TU 里展开时直接访问 `DatabaseState`。模板只负责类型映射；访问 `app_state.database`、检查 executor、入队、等待结果等逻辑放在 `database.cpp` 的非模板 `run_database_job()`。

两件事常一起出现，但动机不同；只改 API 不够时，还要管 **模板实例化发生在哪个 .obj**。优先目标是：**调用方 TU 实例化模板时，不需要看见任何子 state 完整类型**。

### 大白话：为什么 Database 模板不再直接访问 `DatabaseState`

1. Gallery 的 `repository.cpp` 调用 `Core::Database::query<T>(app_state, …)` 时，编译器会在 **repository 那个 .cpp 对应的对象文件里** 把模板展开。
2. 若展开后的模板代码里写 `app_state.database->is_running`，调用方 TU 只有 `Core.State` 的前向声明，看不见 `Core.Database.State::DatabaseState` 的完整布局，就会触发 C2027/C2039 或后续链接问题。
3. 因此现在的边界是：
   - `database.ixx`：保留 `query<T>` / `query_scalar<T>` / `execute_transaction<Func>`，只做 SQL 结果到 `T` 的映射、返回类型推导、事务 lambda 包装。
   - `database.cpp`：实现非模板 `run_database_job(AppState&, DatabaseJob)`，内部可以 import `Core.Database.State`，安全访问 `app_state.database`、`is_running`、任务队列、`current_connection`。

可记一句：**export 模板可以依赖 AppState 作为上下文，但模板体不要穿透 AppState 去摸子 state；需要摸子 state 时，先下沉到同模块 `.cpp` 的非模板函数。**

### 和 Core.Events 的对比

| | Core.Events | Core.Database |
|---|-------------|----------------|
| export 模板在 | `events.ixx` | `database.ixx` |
| 模板里只调 | `send_event(app_state, …)`（不碰 `EventsState&`） | `run_database_job(app_state, …)`（不碰 `DatabaseState&`） |
| 重活在哪 | `events.cpp` 的 **非模板** 函数 | `database.cpp` 的 **非模板** 执行器函数 |
| 子 state 访问在哪 | `events.cpp` | `database.cpp` |

`initialize` / `shutdown` / `execute` 等非模板函数，可以继续放在 `.cpp` 或在 `.cpp` 中委托执行器；一般不会有上述模板实例化问题。

### 什么时候才考虑 `.ixx + inline` helper

只有在模板必须调用一个 helper，且这个 helper 也必须在调用方 TU 展开时可见，同时它又不访问任何子 state 完整布局时，才考虑把 helper 放在 `.ixx` 并标 `inline`。如果 helper 要访问 `state.foo->bar`，优先改成 `.cpp` 非模板执行器，而不是继续扩大 `.ixx` 的状态依赖。

## 跨模块 export API 约定

对外 **export** 的入口函数统一使用 `Core::State::AppState&`（或 `const` 重载）。实现里通过 `state.xxx->…` 直接访问子 state；export 模板仍不要穿透 `AppState` 访问子 state，需要时委托给同模块 `.cpp` 的非模板函数。

已对齐示例：

- `Core.Database`：对外 API 与 `query` / `execute_transaction` 等模板在 `database.ixx`；模板只做类型映射并调用非模板 `run_database_job()`；`DatabaseState` 访问、`current_connection`、任务入队和等待逻辑都在 `database.cpp`。
- `Features.Recording`：对外 `recording.ixx` 与各子模块 export API 均用 `AppState&`；实现见 `Features.Recording.UseCase`（`usecase.cpp`）等 `.cpp` 中的直接 `state.recording->…` 访问。
- `Features.Settings.Menu`、`Features.Preview.Window`：菜单预设与预览窗口尺寸 API 已改为 `AppState&`。

**不要**为修链接或图省事，在 `app_state.ixx` 恢复 `import` 各 `X.State`——会重新接上雪崩重编链。

## 参考

- 项目根目录 `AGENTS.md`：架构与 `AppState` 角色
- 范例模块：`Core.Async`、`Core.I18n`（`AppState&` + `.cpp` 里 `state.xxx->…`）；`Core.Events`（模板 + `.cpp` 非模板）；`Core.Database`（模板映射 + `.cpp` 非模板执行器）；`Features.Recording.UseCase`（feature 侧直接访问范例）
