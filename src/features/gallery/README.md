# Gallery 模块

`gallery` 是项目的核心纵向模块之一。它不只是一个页面，而是同时覆盖后端索引/扫描/监听/静态文件暴露，以及前端浏览/筛选/详情/灯箱等完整流程。

## 模块定位

- 管理图库资产索引与文件动作（扫描、移动、回收站、缩略图）。
- 向上提供 `gallery.*` RPC 能力，向下对扩展提供变化语义（`ScanChange`）。

## 架构概览

### 后端

- `src/features/gallery/gallery.ixx/.cpp`：模块编排入口，负责初始化、清理、扫描、缩略图维护、文件动作、watcher 注册。
- `src/features/gallery/state.ixx`：gallery 运行时状态，例如缩略图目录、资产路径 LRU 缓存、每个根目录的 watcher 状态。
- `src/features/gallery/scanner/`：全量扫描与索引更新（编排 + Common/Progress/Discovery/Analysis/Process/Cleanup + AssetPipeline）。
- `src/features/gallery/watcher/`：目录监听与同步（`watcher` 生命周期编排、`notify` 听盘、`sync` 防抖/增量/全量；物化走 Scanner.AssetPipeline）。
- `src/features/gallery/static_resolver.*`：对外暴露缩略图与原图静态路径。
- `src/features/gallery/asset/`：资产查询、时间线、描述、颜色、缩略图、无限暖暖扩展元数据。
- `src/features/gallery/folder/`：目录树持久化、显示名、根目录 watch 管理。
- `src/features/gallery/tag/`：标签树 CRUD 与资产标签关系。
- `src/features/gallery/ignore/`：扫描忽略规则。
- `src/features/gallery/color/`：主色提取与颜色筛选。

### 前端

- `web/src/features/gallery/api.ts`：gallery RPC 与静态 URL 入口。
- `web/src/features/gallery/store/index.ts`：gallery UI 状态单一事实来源。
- `web/src/features/gallery/store/`：查询、导航、交互等状态切片。
- `web/src/features/gallery/composables/`：数据加载、选择、布局、侧栏、灯箱、资产动作等行为协调。
- `web/src/features/gallery/pages/GalleryPage.vue`：三栏主页面壳。
- `web/src/features/gallery/components/`：viewer、shell、asset、tags、folders、dialogs、lightbox 等 UI 结构。
- `web/src/features/gallery/routes.ts`：`/gallery` 路由定义。

## RPC 与通知

- Gallery RPC 按职责拆在 `src/core/rpc/endpoints/gallery/`：
  - `gallery.cpp`：扫描与维护动作
  - `asset.cpp`：资产查询与文件动作
  - `folder.cpp`：目录树与导航动作
  - `tag.cpp`：标签管理
- 前端通常通过 `gallery.*` RPC 进入 gallery。
- 后端在扫描或索引变更后发送 `gallery.changed`，前端据此刷新目录树与当前视图。

## 先读这些文件

- `src/features/gallery/gallery.cpp`：模块编排入口与资产动作主流程。
- `src/features/gallery/watcher.cpp`：目录监听、增量同步、扫描后回调。
- `src/features/gallery/scanner/scanner.cpp`：全量扫描编排入口（五阶段伪代码级流程）。
- `src/features/gallery/scanner/asset_pipeline.*`：单路径 prepare/upsert/remove（全量与增量共用）。
- `src/features/gallery/watcher/watcher.cpp`：注册/启停/startup recovery/manual move。
- `src/features/gallery/watcher/notify.*`：ReadDirectoryChanges → 入队。
- `src/features/gallery/watcher/sync.*`：pending 队列、防抖、增量/全量应用、dispatch。
- `src/features/gallery/types.ixx`：`ScanResult` / `ScanChange` / `OperationResult` 等稳定语义。
- `src/core/rpc/endpoints/gallery/asset.cpp`：前端资产动作 RPC 入口。
- `web/src/features/gallery/store/index.ts`：前端状态入口。
- `web/src/features/gallery/composables/`：前端行为组织入口。

## 核心数据流

- 文件变化（watcher 或手动动作） -> `ScanChange` -> 回调消费者（例如扩展派生同步）。
- 前端调用 `gallery.*` RPC -> gallery 服务/仓储 -> 返回查询结果或操作结果。
- 扫描或动作完成 -> `gallery.changed` 通知前端刷新。

## 启动行为

- 应用启动时，gallery 会先完成模块初始化。
- 随后从数据库恢复 watcher 注册。
- 再执行 Infinity Nikki 照片源注册。
- 所有已注册 watcher 会在启动靠后阶段统一启动。

## 关键不变量

- `ScanChange` 是“文件变化事实”，用于派生同步，不等同于 UI 提示或 DB 统计。
- 手动文件动作若绕开 watcher 事件，必须显式补发 `ScanChange`。
- `watcher` 增量整体失败时自动回退一次全量；全量仍失败则进入 Faulted，等待用户明确重试。
- 启动恢复先让实时通知入队，再应用 USN/全量基线；checkpoint 只推进到已成功应用的启动边界。
- Scanner 只更新文件系统派生字段，不能通过整行 Asset 写回覆盖用户编辑字段。
- 缩略图按内容 hash 共享；删除资产时不即时删图，孤儿统一由缓存对账回收。
- 每个 root 只保留一条目录监听线程；Gallery 用一条全局编排线程串行消费各 root 的同步事实。
- 每个 root 的 watcher 状态直接归 GalleryState 所有；后台入口按路径 key 重新定位状态。
- 前端应优先遵循 `component -> composable -> api -> RPC` 的既有数据流，不要在组件内复制 store 状态。
- `web/src/features/gallery/store/index.ts` 是 gallery UI 状态的单一事实来源。

## 改动提示

- 改 `move_assets_to_trash` / `move_assets_to_folder`：
  - 确认 watcher ignore 是否会吞掉系统事件。
  - 确认是否补发了正确的 `REMOVE/UPSERT` changes。
- 改 `watcher` 分发逻辑：
  - 确认 `post_scan_callback` 仍能收到变化。
  - 确认不会引入重复 `gallery.changed` 通知来源。
- 改 `types.ixx` 中扫描/操作结果结构：
  - 同步检查 RPC 端和扩展消费者是否受影响。
- 改筛选/排序/视图模式：
  - 先看 `store/index.ts`、`store/` 下相关 slice，以及 `queryFilters.ts`。
- 改可见交互行为：
  - 先看对应 composable，再动大型 Vue 组件。

## 不要做什么

- 不要在 gallery 内直接写 Infinity Nikki 业务分支。
- 不要把“是否需要通知扩展”与“是否需要刷新前端 UI”混成一个开关。
