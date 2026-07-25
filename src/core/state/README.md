# core::AppState

本文档说明 `AppState` 的布局、头文件依赖边界和后续维护约定。

## 结构

| 文件 | 职责 |
|------|------|
| `app_state.hpp` | 声明 `core::AppState`，通过前向声明持有各子系统 state |
| `app_state.cpp` | 包含各 `state.hpp` 完整定义，集中构造和析构所有子 state |

各子系统 state 直接放在所属领域命名空间，例如：

```cpp
namespace features::gallery {
struct GalleryState;
}
```

不要因为文件名是 `state.hpp` 再创建 `features::gallery::state` 命名空间。

## 为什么继续使用前向声明

`app_state.hpp` 被大量头文件和实现文件包含。若它直接包含所有子系统 `state.hpp`，任意 state
布局变化都会使所有间接消费者重新编译。

因此：

- `app_state.hpp` 只前向声明子 state，并保存 `std::unique_ptr<T>`。
- `app_state.cpp` 包含所有完整 state 定义，负责 `std::make_unique` 和析构。
- PCH 只能加速外部依赖，不能用来隐藏项目头文件依赖。

这既降低重编范围，也使 `AppState` 保持稳定的聚合边界。

## API 约定

跨子系统入口优先接受：

```cpp
core::AppState&
const core::AppState&
```

实现文件再通过 `app_state.gallery->...` 等成员访问具体状态。调用方不应穿透
`AppState` 后把某个子 state 传给另一个领域。

应用完成初始化后的正常运行路径中，各 state 指针均由 `app_state.cpp` 创建，不需要在每个
业务函数里重复判空。

## 模板与非模板执行器

头文件模板会在调用方翻译单元实例化。若模板体直接访问某个子 state 的字段，调用方必须包含
该 state 的完整定义，容易扩大依赖范围。

模板应只完成类型映射，并把需要访问具体 state、持锁或调度线程的工作委托给 `.cpp` 中的
非模板函数。例如：

- `core::events` 模板调用非模板事件执行器。
- `core::database` 查询模板调用非模板数据库任务执行器。

只有真正依赖模板参数的逻辑才放在头文件里。

## 新增子 state

1. 在所属领域的 `state.hpp` 中定义 `XxxState`。
2. 在 `app_state.hpp` 的所属领域命名空间前向声明它。
3. 在 `AppState` 中增加 `std::unique_ptr<XxxState>`。
4. 在 `app_state.cpp` 包含对应 `state.hpp` 并集中构造。
5. 对外能力继续接受 `core::AppState&`，不要暴露不必要的具体 state 依赖。

## 头文件自包含

每个项目头文件必须：

- 显式包含 `vendor/std.hpp`；
- 显式包含所需第三方 facade；
- 显式包含所需项目头；
- 在禁用 PCH 时能够独立作为第一个 include 编译。

`src/pch.hpp` 是构建优化，不是正确性依赖。
