# 地图模块（Map）维护说明

面向后续开发与 AI 助手的简版架构与现状说明。详细构建方式见仓库根目录 `AGENTS.md`。

## 产品定位

- 在 **内嵌 WebView/浏览器 iframe** 中加载官方工具站地图：`MAP_URL` / `MAP_ORIGIN` 定义在 [`bridge/protocol.ts`](./bridge/protocol.ts)（`myl.nuanpaper.com`）。
- 本应用不托管地图，而是通过 **postMessage** 把「图库中照片的坐标 + 展示配置」同步到 iframe 内由注入脚本在 Leaflet 上画标点、聚合、悬停卡片，并支持从卡片跳转图库。

## 前端目录职责

| 路径                            | 作用                                                                                                                 |
| ------------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| `pages/MapPage.vue`             | 地图页 UI 与 `useMapScene` 初始化                                                                                    |
| `store.ts`                      | `markers`、`renderOptions`、`runtimeOptions` 等轻量状态                                                              |
| `composables/useMapScene.ts`    | 监听图库筛选/排序/语言，拉取点位、写入 `mapStore`                                                                    |
| `composables/useMapBridge.ts`   | iframe `postMessage`：prod 为 `SYNC_RUNTIME`；dev 为 `EVAL_SCRIPT` + 入站（打开图库/标点显隐）                       |
| `bridge/protocol.ts`            | 与 iframe 的 action 常量、payload 类型；生产同步为 `SPINNING_MOMO_SYNC_RUNTIME`，Vite dev 另发 `EVAL_SCRIPT`（见下） |
| `injection/mapDevEvalScript.ts` | **仅 dev**：把当前 store 快照拼成 iframe 内 eval 用整段脚本                                                          |
| `domain/*`                      | 与 Vue 无关的纯逻辑：坐标、默认配置、**PhotoMapPoint → MapMarker**                                                   |
| `api.ts`                        | 对 `gallery.queryPhotoMapPoints` 的 feature 内门面，避免页面直接依赖 gallery API                                     |
| `components/MapIframeHost.vue`  | 全局布局里常驻的 iframe 容器，避免切路由时地图白屏重载；内部极薄，委托 `useMapBridge`                                |

## 数据模型（宿主 → iframe 必须可结构化克隆）

`MapMarker` 为 **扁平** 结构（无嵌套 `popup`），见 [`store.ts`](./store.ts)：

- 坐标：`lat` / `lng`（已按本模块约定从游戏坐标换算为地图坐标）
- 展示：`cardTitle`（悬停标题，已按 i18n 在 mapper 中格式化）
- 资源与跳转：`assetId` / `assetIndex` / `thumbnailUrl`（与图库、lightbox 一致）
- 悬停总开关在 **`MapRuntimeOptions.hoverCardEnabled`**（单点 + 聚合同一语义；不再使用第二套 `openPopupOnHover`）
- 延时/移出行为在 **`MapRenderOptions`**（如 `popupOpenDelayMs`、`closePopupOnMouseOut` 等），仅表示交互参数，不重复表达「开不开悬停」

`useMapBridge` 出站前会把 Pinia 状态**手工展开为纯对象**再 `postMessage`，避免 `DataCloneError`。

## iframe 内注入（核心）

- **源码**：`injection/source/*.js` 以「字符串模板」拼进 `runtimeCore`；修改后**必须**从仓库根目录执行：  
  `node scripts/generate-map-injection-cpp.js`  
  以更新 C++ 嵌入的 [`map_injection_script.ixx`](../../../src/extensions/infinity_nikki/generated/map_injection_script.ixx)（供打包后的 Win32 注入用）。
- **prod**：宿主 → iframe 仅 `SPINNING_MOMO_SYNC_RUNTIME`，`payload` 为 `{ markers, renderOptions, runtimeOptions }`（与 `useMapBridge` 序列化一致）。
- **Vite dev**：同一 payload 由 [`injection/mapDevEvalScript.ts`](./injection/mapDevEvalScript.ts) 拼成整段 IIFE，经 `EVAL_SCRIPT` 在 iframe 内 `new Function` 执行，便于**不重新生成 C++ 嵌入串**即可热更 `injection/source`；仅当注入脚本里 `__ALLOW_DEV_EVAL__` 被 C++ 换为 `true`（Debug 构建）时生效，Release 为 `false`。
- **桥接入口**：[`injection/source/bridgeScript.js`](./injection/source/bridgeScript.js) 只拼两层：**[`iframeBootstrap.js`](./injection/source/iframeBootstrap.js)**（脚本加载后即可跑的页壳，当前含侧栏自动收起）+ **`runtimeCore`**（`mountOrUpdateMapRuntime` 及其子 snippet）。Vite dev 的 `EVAL_SCRIPT` 包（[`devEvalRuntimeScript.js`](./injection/source/devEvalRuntimeScript.js)）**不含** bootstrap，只热更 `runtimeCore`。
- **拼入顺序**（[`runtimeCore.js`](./injection/source/runtimeCore.js)）：`paneStyle` → `popup`（悬停层、计时、escapeHtml）→ `photoCardHtml` → `cluster` → `toolbar` → `render`。

### 子模块分工（注入侧）

- **paneStyle.js**：地图容器上自定义标点共用的 `spinning-momo-photo-pane` 与弹层 CSS（含单图 `thumbnail-image` 的 max 尺寸，横竖图适配）。
- **photoCardHtml.js**：`buildPhotoThumbCellHtml`（**聚合**方格，1:1 + cover）；`buildSinglePhotoHoverHtml`（**单点**，恢复 `thumbnail-block` / `thumbnail-image` 类以沿用 CSS）。
- **popup.js**：悬停卡片容器定位、`scheduleOpenHoverCard` / `showHoverCard`、`bindPopupCardClickBridge`（`data-sm-open-asset-id` 点击 → `SPINNING_MOMO_OPEN_GALLERY_ASSET`）；`activeHoverCardContext` 含 **`latLng`**，供 **`refreshActiveHoverCardPosition`** 在内容变高后重算锚点（聚合展开等场景）。
- **render.js**：单点 Leaflet 标点与 hover 绑定（受 `hoverCardEnabled` 控制）。
- **cluster.js**：网格聚合；预览网格带 **`data-sm-cluster-grid-root`**，卡片根带 **`data-sm-cluster-card`**。点「+N 更多」时 **增量 DOM**：去掉 `[data-sm-cluster-expand]`、向同一 grid **append** 剩余缩略图、再包 **`data-sm-cluster-scroll`** 与滚轮穿透处理，**不**整卡 `innerHTML` 替换，避免预览缩略图闪烁；`smClusterExpanded` 防重复展开。
- **iframeBootstrap.js**：与 **map 实例无关** 的第三方页壳（侧栏收起等）；**toolbar.js**：在 **`mountOrUpdateMapRuntime`** 内挂按钮，需读 `runtime` / 与标点显隐同步。二者均属脆弱 DOM 适配。

## 与图库联动

- 地图点来自 RPC（经 `api.ts`）：仍基于当前图库筛选/排序，保证 `assetIndex` 与图库 lightbox 一致。
- 卡片内点击经 `useMapBridge` 收到后：`galleryStore` 设活跃资产、打开 lightbox、路由到 gallery。

## 常见修改点

- 只改数据形状：先改 `store` + `domain/markerMapper.ts` + `useMapBridge` 序列化，再改注入里读取字段。
- 只改单图/聚合格式：先改 `photoCardHtml.js` 或 `paneStyle.js`；改聚合展开/预览逻辑看 **`cluster.js`**；记得跑 **generate-map-injection**。
- 不要恢复「`popup` 存在才绑 hover」这类隐式条件；单点与聚合的开关统一用 `hoverCardEnabled`。
