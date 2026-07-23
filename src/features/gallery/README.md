# Gallery 模块

Gallery 负责把文件系统中的照片和视频维护为可查询的图库索引，并提供文件夹、标签、
缩略图、筛选、文件操作及变化通知。它不托管或备份原件：数据库保存图库元数据，
文件系统决定原件当前是否可用。

## 模块边界

- Gallery 向前端提供 `gallery.*` RPC，并在可见索引变化后发送 `gallery.changed`。
- Gallery 通过 `ScanChange` 向扩展报告文件路径变化，但不包含 Infinity Nikki 等扩展业务。
- 前端沿用 `component -> composable -> store/api -> RPC` 数据流，
  `web/src/features/gallery/store/index.ts` 是 Gallery UI 状态入口。

## 后端入口

- `gallery.cpp`：模块初始化、清理及应用主动发起的文件操作。
- `scanner/scanner.cpp`：全量扫描编排。
- `scanner/asset_pipeline.cpp`：全量和增量共用的单路径资产处理。
- `watcher/watcher.cpp`：watcher 注册、生命周期和启动恢复。
- `watcher/notify.cpp`：接收文件系统通知并写入待处理队列。
- `watcher/sync.cpp`：防抖、增量同步、全量回退和结果分发。
- `asset/`、`folder/`、`tag/`、`color/`：索引查询与各自的数据操作。
- `asset/thumbnail.cpp`：缩略图生成、修复和缓存对账。
- `static_resolver.cpp`：缩略图与原图的静态访问入口。
- `types.ixx`：跨扫描器、watcher、RPC 和扩展共享的稳定语义。

## 资产身份

`assets` 的一行表示一个已索引路径下的资产副本。路径和 hash 承担不同职责：

- 相同路径重新出现或内容发生变化，视为同一资产，沿用原 `id`。
- 新路径始终创建新资产行；若 hash 已存在，则从相同 hash 中最早的 `id` 一次性继承用户数据。
- 相同 hash 可以对应多个资产行。继承完成后，各副本的用户数据可以独立变化，不持续同步。
- 因此，路径维持同一位置上的编辑连续性，hash 只用于新路径之间的内容等价与数据继承。

新路径继承的用户数据包括：

- `description`、`rating`、`review_flag`
- 标签关系
- 扩展通过继承回调维护的资产数据

路径、文件时间、大小、媒体信息和主色等 Gallery 派生数据必须根据新文件重新生成。
缩略图按 hash 共享；Infinity Nikki 扩展会为相同 hash 新资产复制用户记录、照片参数和服装关系，
使复制资产立即拥有与来源资产相同的完整暖暖信息。

## Missing 生命周期

文件系统中的路径消失不等于用户决定删除图库数据：

```text
外部路径消失
→ 设置 missing_at（重复事件不重置时间）
→ 普通图库查询隐藏该资产

原路径重新出现
→ 清空 missing_at
→ 更新同一资产行并保留用户数据

新路径出现且 hash 已存在
→ 创建新资产行
→ 从最早的同 hash 资产继承用户数据

所有启动恢复完成或尝试完成
→ 硬删除 missing 超过 30 天的资产
→ 执行全局缩略图缓存对账
```

资源管理器删除、移动、watcher `REMOVE` 和全量扫描对账都只会使资产进入 missing。
应用内明确移入回收站、删除资产或移除监控根仍然立即硬删除，因为这些操作表达了用户意图。

`ScanChange::REMOVE` 表示路径已经从磁盘消失，不表示数据库行已被删除；
`ScanResult::missing_items` 表示本轮首次进入 missing 的资产数量。

## 查询与缩略图

- 常规资产、时间线、统计、文件夹、标签及扩展候选查询默认只包含 `missing_at IS NULL`。
- missing 资产在宽限期内仍引用原缩略图，不能被孤儿清理提前删除。
- 缩略图修复只使用当前存在的原件；30 天回收后，无引用缩略图由同一次启动对账清理。
- 缩略图按内容 hash 共享，单个资产状态变化不能直接删除共享文件。

## 启动恢复与根目录状态

启动顺序为：恢复 watcher 注册、注册扩展回调、逐个 root 执行 USN 恢复或全量扫描、
回收过期 missing 资产，最后进行全局缩略图对账。

不可达的网络根会跳过 watcher 和启动扫描，因此不会仅因根不可达而把其资产批量标记 missing。
每个 root 只有一条监听线程，Gallery 使用一条全局同步线程串行消费各 root 的待处理变化。
增量同步整体失败时回退一次全量扫描；全量仍失败则将该 root 标为 Faulted，等待用户重试。

## 必须保持的不变量

- Scanner 只能更新文件系统或媒体派生字段，不能覆盖资产用户数据。
- 全量扫描与 watcher 必须复用 `Scanner.AssetPipeline` 的路径处理语义。
- 目录库存变化可以刷新 Gallery UI，但不能伪造文件级 `ScanChange`。
- 应用主动文件操作若忽略了对应 watcher 事件，必须显式补发真实的 `REMOVE/UPSERT`。
- 启动恢复先接收实时通知，再应用 USN 或全量基线；checkpoint 只能推进到成功应用的边界。
- Gallery 不得查询或修改扩展业务表；扩展通过回调和 `ScanChange` 维护自己的数据。
