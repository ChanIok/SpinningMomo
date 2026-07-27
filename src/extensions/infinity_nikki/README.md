# Infinity Nikki 扩展说明（AI 速读）

本文件只补充 `AGENTS.md` 之外的 `infinity_nikki` 扩展信息，重点是核心链路与改动注意点。

## 扩展定位

- 提供《无限暖暖》领域能力：照片目录接入、账号昵称、硬链接同步、参数提取、地图扩展。
- 通过通用 `gallery` 变化语义接收文件变化，不要求基础模块理解游戏规则。

## 先读这些文件

- `src/extensions/infinity_nikki/photo_service.cpp`：监听注册与扫描后回调接线入口。
- `src/extensions/infinity_nikki/role_profile.cpp`：识别新建 UID 目录，并异步补全账号昵称。
- `src/extensions/infinity_nikki/media_hardlinks.cpp`：Infinity Nikki 照片/录像受管硬链接同步核心。
- `src/extensions/infinity_nikki/task_service.cpp`：扩展任务编排与进度上报。
- `src/extensions/infinity_nikki/types.hpp`：扩展请求/结果结构。
- `src/core/initializer/initializer.cpp`：扩展在启动流程中的接入时机。

## 核心链路

- 设置驱动注册 watcher -> `PhotoService` 绑定 `post_scan_callback`。
- callback 收到 `ScanResult.changes` -> `media_hardlinks::apply_runtime_changes`（增量）。
- 在线照片元数据已授权时，callback 从 `ScanResult.created_folders` 筛选直属 UID 新目录
  -> 请求角色昵称 -> 补全该目录的 `display_name`。
- 变更集不可用或初始化场景 -> `media_hardlinks::sync/initialize`（全量）。
- 重操作统一通过 `task_service` 进入任务系统，便于前端观测进度。

## 关键不变量

- Infinity Nikki 的媒体硬链接目录是受管镜像，不是独立来源目录。
- 运行时优先增量应用，失败或缺失时可回退全量重建。
- 变更消费基于 gallery 的 `ScanChange` 语义，避免依赖某个具体 RPC 用例。
- 角色昵称只补全空的 `display_name`，不能覆盖用户手动名称；查询失败不影响图库扫描。
- 角色昵称请求与 `allow_online_photo_metadata_extract` 共用在线授权，关闭后不得发送 UID。

## 改动 checklist

- 改硬链接规则（文件名映射、目录规则、删除逻辑）：
  - 同时检查增量路径 `apply_runtime_changes` 与全量路径 `sync/initialize`。
- 改照片监听注册逻辑：
  - 检查 `register_from_settings` / `refresh_from_settings` / `shutdown` 是否一致。
  - 检查回调是否仍绑定在正确 watcher root。
- 改角色昵称同步：
  - 检查只消费当前 `GamePlayPhotos` 的直属数字 UID 新目录，并发创建按 UID 去重。
  - 检查数据库仍以条件更新保护用户手动名称，成功更新后发送 `gallery.changed`。
- 改任务流程：
  - 检查 task type、进度字段与失败摘要文案是否仍兼容前端任务面板。

## 不要做什么

- 不要让 gallery 直接感知 Infinity Nikki 业务分支。
- 不要只修增量不同步而忽略全量重建路径。
