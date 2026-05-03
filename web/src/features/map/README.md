# 地图模块（Map）维护说明

简版架构说明；构建见仓库根目录 `AGENTS.md`。

## 产品定位

- 在 **WebView / iframe** 中加载官方地图（`MAP_URL` / `MAP_ORIGIN` 见 [`bridge/protocol.ts`](./bridge/protocol.ts)）。
- 宿主通过 **postMessage** 把「当前区域照片坐标 + 展示配置」同步进 iframe；注入脚本在 Leaflet 上画点、聚合、悬停卡片；卡片可跳回图库。
- 地图世界列表、区域 polygon、zRange、官方 `worldId` 版本和坐标转换参数来自后端远端 JSON 配置，不再由前端或客户端 C++ 硬编码。

## 宿主数据流（最短路径）

1. iframe 上报 **`SPINNING_MOMO_MAP_SESSION_READY`**，payload 带 `worldId`（来自 `localStorage.infinitynikkiMapState-v2.state.currentWorldId`；读取失败或无效时回退 `1.1`，并去掉首尾多余 `"`）。
2. [`useMapBridge`](./composables/useMapBridge.ts)：`normalizeOfficialWorldId` → 写入 `runtimeOptions.currentWorldId` → **`markIframeSessionReady()`** → **`flushMapRuntimeToIframe()`**（切换 world 时补一帧同步）。
3. [`useMapScene`](./composables/useMapScene.ts) 监听筛选/排序/语言及 **`iframeSessionReady` + `currentWorldId`**：二者齐全才 **`extensions.infinityNikki.queryPhotoMapPoints`（带 `worldId`）**；后端按远端地图配置完成区域过滤和坐标转换，前端直接使用返回的 `lat/lng`，再 `replaceMarkers` → **`flushMapRuntimeToIframe()`**；未就绪时清空点位并显示「等待地图区域就绪」类文案。
4. [`mapIframeRuntime.ts`](./composables/mapIframeRuntime.ts) 只负责注册 **`flush` → `postRuntimeSync`**，无第二套回调。

## 远端地图配置

- 后端入口：`Extensions.InfinityNikki.WorldArea` 请求 `https://api.infinitymomo.com/api/v1/map.json`，解析后以内存 TTL 缓存。
- JSON 是地图数据的唯一来源，客户端不再保留旧区域硬编码；首次加载失败时地图点位查询失败并显示空点位，详情页只省略区域增强信息。
- 进程内已有成功配置后，后续刷新失败会继续使用上一份远端缓存。
- 配置 JSON 使用 camelCase；至少包含：`schemaVersion`、`defaultWorldId`、`worlds[]`；每个 world 包含 `worldId`、`officialWorldId`、`name`、`coordinate`、`rules[].polygon` 与可选 `rules[].zRange`。C++ 内部结构仍使用 snake_case，通过 `rfl::SnakeCaseToCamelCase` 解析。

## 前端目录职责

| 路径                              | 作用                                                                                                                         |
| --------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `pages/MapPage.vue`               | 挂载时 `initializeMapDefaults()`                                                                                             |
| `store.ts`                        | `markers`、`renderOptions`、`runtimeOptions`、`iframeSessionReady`                                                           |
| `composables/useMapScene.ts`      | 图库筛选/排序/语言 + 地图 session/world 就绪后拉点、写 markers、flush                                                        |
| `composables/useMapBridge.ts`     | 出站 `postMessage`（prod `SYNC_RUNTIME`，dev `EVAL_SCRIPT`）；入站处理；`SESSION_READY` 写 world、标记 session，并立即 flush |
| `composables/mapIframeRuntime.ts` | 注册宿主 → iframe 的 flush 实现                                                                                              |
| `domain/officialWorldId.ts`       | 规范化 iframe 传来的 world 字符串（如去掉包裹 `"`）；不保存区域列表或版本映射                                                |
| `bridge/protocol.ts`              | action 常量与 payload 类型                                                                                                   |
| `injection/mapDevEvalScript.ts`   | **仅 dev**：把 store 快照拼成 iframe 内 eval 脚本                                                                            |
| `domain/*`                        | 默认展示配置、远端坐标配置下的 polygon 导出反算、`PhotoMapPoint` → `MapMarker`                                               |
| `api.ts`                          | 对 `extensions.infinityNikki.queryPhotoMapPoints` 的门面                                                                     |
| `components/MapIframeHost.vue`    | 全局 iframe 容器、`registerMapIframeFlush`、监听 `message`                                                                   |

## 数据模型（宿主 → iframe，须可结构化克隆）

见 [`store.ts`](./store.ts)：`MapMarker` 扁平结构；`useMapBridge` 出站前手工展开为纯对象，避免 `DataCloneError`。

- 悬停总开关：**`MapRuntimeOptions.hoverCardEnabled`**
- 交互延时等：**`MapRenderOptions`**（与「是否启用悬停」分离）

## iframe 注入

- **源码**：`injection/source/*.js` 拼进 `runtimeCore`；改后执行：  
  `node scripts/generate-map-injection-cpp.js`  
  更新 [`map_injection_script.ixx`](../../../src/extensions/infinity_nikki/generated/map_injection_script.ixx)。
- **prod**：`SPINNING_MOMO_SYNC_RUNTIME`，payload `{ markers, renderOptions, runtimeOptions }`。
- **dev**：同 payload 经 `mapDevEvalScript` 包成 `EVAL_SCRIPT`；`postMessage` 的 targetOrigin 为 `*`，与 Vite 环境一致。
- **桥接**：[`bridgeScript.js`](./injection/source/bridgeScript.js) = `iframeBootstrap` + `runtimeCore`；dev 的 [`devEvalRuntimeScript.js`](./injection/source/devEvalRuntimeScript.js) 不含 bootstrap。
- **拼入顺序**（[`runtimeCore.js`](./injection/source/runtimeCore.js)）：`paneStyle` → `popup` → `photoCardHtml` → `cluster` → **`worldIdBridge`** → `toolbar` → `render`。

### 注入子模块（简述）

- **paneStyle / photoCardHtml / popup / cluster / render**：样式、卡片 HTML、悬停层、聚合、单点渲染（细节见各文件头注释）。
- **worldIdBridge.js**：路由/history 变化时同步 `infinitynikkiMapState-v2.state.currentWorldId` 并向宿主发 `SESSION_READY`（失败回退 `1.1`）。
- **iframeBootstrap.js**：与地图实例无关的页壳（如侧栏）。
- **toolbar.js**：工具条 + 右上角筛选计数卡片（读 `runtimeOptions` 中文案与 loading）。

## 与图库联动

- 点位 RPC 带当前图库筛选/排序及 **`worldId`**，与 lightbox 序号一致；RPC 返回的点位已经包含地图 `lat/lng`。
- 卡片点击 → `useMapBridge` → 设活跃资产、开 lightbox、跳转 gallery。

## 常见修改点

- 改数据形状：`store` → `domain/markerMapper.ts` → `useMapBridge` 序列化 → 注入侧读字段。
- 改卡片/聚合 UI：`photoCardHtml.js` / `paneStyle.js` / `cluster.js`；改完跑 **generate-map-injection**。
- 悬停开关统一用 `hoverCardEnabled`，勿再叠隐式「有 popup 才绑 hover」。
