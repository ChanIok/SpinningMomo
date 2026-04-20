# Gallery 模块说明（AI 速读）

本文件只补充 `AGENTS.md` 之外的 gallery 模块增量信息，聚焦“先看哪里、改动时别踩什么坑”。

## 模块定位

- 管理图库资产索引与文件动作（扫描、移动、回收站、缩略图）。
- 向上提供 `gallery.*` RPC 能力，向下对扩展提供变化语义（`ScanChange`）。

## 先读这些文件

- `src/features/gallery/gallery.cpp`：模块编排入口与资产动作主流程。
- `src/features/gallery/watcher.cpp`：目录监听、增量同步、扫描后回调。
- `src/features/gallery/scanner.cpp`：全量扫描主逻辑。
- `src/features/gallery/types.ixx`：`ScanResult` / `ScanChange` / `OperationResult` 等稳定语义。
- `src/core/rpc/endpoints/gallery/asset.cpp`：前端资产动作 RPC 入口。

## 核心数据流

- 文件变化（watcher 或手动动作） -> `ScanChange` -> 回调消费者（例如扩展派生同步）。
- 前端调用 `gallery.*` RPC -> gallery 服务/仓储 -> 返回查询结果或操作结果。
- 扫描或动作完成 -> `gallery.changed` 通知前端刷新。

## 关键不变量

- `ScanChange` 是“文件变化事实”，用于派生同步，不等同于 UI 提示或 DB 统计。
- 手动文件动作若绕开 watcher 事件，必须显式补发 `ScanChange`。
- `watcher` 增量失败时允许回退全量，目标是最终一致性而非单次完美增量。

## 改动 checklist

- 改 `move_assets_to_trash` / `move_assets_to_folder`：
  - 确认 watcher ignore 是否会吞掉系统事件。
  - 确认是否补发了正确的 `REMOVE/UPSERT` changes。
- 改 `watcher` 分发逻辑：
  - 确认 `post_scan_callback` 仍能收到变化。
  - 确认不会引入重复 `gallery.changed` 通知来源。
- 改 `types.ixx` 中扫描/操作结果结构：
  - 同步检查 RPC 端和扩展消费者是否受影响。

## 不要做什么

- 不要在 gallery 内直接写 Infinity Nikki 业务分支。
- 不要把“是否需要通知扩展”与“是否需要刷新前端 UI”混成一个开关。
