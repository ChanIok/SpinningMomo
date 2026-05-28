# AGENTS.md

This file provides guidance to AI when working with code in this repository.

## 第一性原理
请使用第一性原理思考。你不能总是假设我非常清楚自己想要什么和该怎么得到。请保持审慎，从原始需求和问题出发，如果动机和目标不清晰，停下来和我讨论。

## 方案规范
当需要你给出修改或重构方案时必须符合以下规范：
- 不允许给出兼容性或补丁性的方案
- 不允许过度设计，保持最短路径实现且不能违反第一条要求

## Project Overview

SpinningMomo (旋转吧大喵) is a Windows-only desktop tool for the game "Infinity Nikki" (无限暖暖), focused on photography, screenshots, recording, and related workflow tooling around the game window. The current repository is a native Win32 C++ application with an embedded web frontend, plus supporting docs, packaging, and playground tooling. The codebase is bilingual — code comments and UI strings are predominantly in Chinese.

## Build & Development

Full setup steps live in `docs/developer/architecture.md`.

Common commands:
```
# C++ backend — debug
xmake build

# C++ backend — release
xmake release

# Web frontend
npm run build --prefix web
```

`web/` uses a Vite dev server and proxies `/rpc` and `/static` to the backend at `localhost:51206`.
`docs/` is a separate VitePress site and is not part of the runtime bundle.

## Architecture

### Two-Process Model
The application is a **native Win32 C++ backend** that hosts an embedded **WebView2** frontend. Communication happens over **JSON-RPC 2.0** through two transport layers:
- **WebView bridge** — used when the Vue app runs inside WebView2 (production)
- **HTTP + SSE** — used when the Vue app runs in a browser during development (uWebSockets on port 51206). SSE provides server-to-client push notifications.

The frontend auto-detects its environment (`window.chrome.webview` presence) and selects the appropriate transport.

### C++ Module System
The backend uses **C++23 modules** (`.ixx` interface files, `.cpp` implementation files). Module names follow a dotted hierarchy that mirrors the directory structure:

- `Core.*` — framework infrastructure (async runtime, database, events, HTTP client, HTTP server, RPC, WebView, i18n, commands, migration, worker pool, tasks, runtime info, shutdown, state)
- `Features.*` — business logic modules such as gallery, letterbox, notifications, overlay, preview, recording, screenshot, settings, update, and window_control
- `UI.*` — native Win32 UI (floating_window, tray_icon, context_menu, webview_window)
- `Utils.*` — shared utilities such as logger, file, graphics, image, media, path, string, system, throttle, timer, dialog, crash_dump, and crypto
- `Vendor.*` — thin wrappers re-exporting Win32 API and third-party types through the module system (e.g. `Vendor.Windows` wraps `<windows.h>`)

### Design Philosophy
The C++ backend does **NOT** use OOP class hierarchies. Instead it follows:
- **POD Structs + Free Functions**: plain data structs with free functions operating on them.
- **Centralized State**: all state lives in `AppState`, passed by reference.
- **Feature Independence**: features depend on `Core.*` but must NOT depend on each other.

### Central AppState
`Core::State::AppState` is the single root state object. It owns all subsystem states as `std::unique_ptr` members. Functions are free functions that accept `AppState&`.

### Key Patterns
- **Error handling**: `std::expected<T, std::string>` throughout; no exception-based control flow.
- **Async**: Asio-based coroutine runtime (`Core::Async`). RPC handlers return `asio::awaitable<RpcResult<T>>`.
- **Events**: Type-erased event bus (`Core::Events`) with sync `send()` and async `post()` (wakes the Win32 message loop via `PostMessageW`).
- **RPC registration**: `Core::RPC::register_method<Req, Res>()` auto-generates JSON Schema from C++ types via reflect-cpp. Field names are auto-converted between `snake_case` (C++) and `camelCase` (JSON).
- **Commands**: `Core::Commands` registry binds actions, toggle states, i18n keys, and optional hotkeys. Context menu and tray icon are driven by this registry.
- **Database**: SQLite via SQLiteCpp with thread-local connections, a `DataMapper` for ORM-like row mapping, and an auto-generated migration system (`scripts/generate-migrations.js`).
- **Vendor wrappers**: Win32 macros/functions are re-exported as proper C++ functions/constants in `Vendor::Windows` to stay compatible with the module system.
- **String encoding**: internal processing uses UTF-8 (`std::string`); Win32 API calls use UTF-16 (`std::wstring`). Convert via utilities in `Utils.String`.

### Web Frontend (web/)
The main frontend lives in `web/` and uses Vue 3 + TypeScript + Pinia + Tailwind CSS v4 + shadcn-vue/reka-ui. It is built with a Vite-compatible toolchain. Key directories:
- `web/src/core/rpc/` — JSON-RPC client with WebView and HTTP transports
- `web/src/core/i18n/` — client-side i18n
- `web/src/core/env/` — runtime environment detection
- `web/src/core/tasks/` — frontend task orchestration
- `web/src/features/` — feature modules (gallery, settings, home, about, map, onboarding, common, playground)
- `web/src/composables/` — shared composables (`useRpc`, `useI18n`, `useToast`)
- `web/src/extensions/` — game-specific integrations (infinity_nikki)
- `web/src/router/` — routes
- `web/src/types/` — shared TS types
- `web/src/lib/` — shared UI/helpers
- `web/src/assets/` — static assets

### Additional Repo Surfaces
- `docs/` — VitePress documentation site for user and developer docs
- `playground/` — standalone Node/TypeScript scripts for backend HTTP/RPC debugging and experiments
- `installer/` — WiX source files for MSI and bundle installer generation
- `tasks/` — custom xmake tasks such as `build-all`, `release`, and `vs`

### RPC Endpoint Organization
Endpoints live under `src/core/rpc/endpoints/<domain>/`. Each domain exposes a `register_all(state)` called from `registry.cpp`.
Game-specific adapters live under `src/extensions/` and are exposed via `rpc/endpoints/extensions/`.

### Initialization Order
Initialization still follows `main.cpp` → `Application::Initialize()` → `Core::Initializer::initialize_application()`.
The rough order is: core infrastructure first, then native UI, then feature services, then extensions and startup tasks.

## Build Output
- Release: `build\windows\x64\release\`
- Debug: `build\windows\x64\debug\`
- Distribution: `dist/` (exe + web resources)

## Installer
Installers are built via `node scripts/build-msi.js` (or `npm run build:installer`). The script builds an MSI package and, by default, a WiX bundle-based setup `.exe`, both under `dist/`. Use `--msi-only` to skip the bundle; use `--version X.Y.Z` to override `version.json`.

## Code Generation Scripts
These must be re-run when their source files change:
- `node scripts/generate-migrations.js` — after modifying `src/migrations/*.sql`
- `node scripts/generate-embedded-locales.js` — after modifying `src/locales/*.json` (zh-CN / en-US)
- `node scripts/generate-map-injection-cpp.js` — after modifying `web/src/features/map/injection/source/*.js` (regenerates minified JS and C++ map injection module)

## Comments
- Comments should describe intent and logic (why / what), not restate what the code already shows (how).
- When changing code, update related comments so they stay in sync with the implementation.

## Naming Conventions
- **C++ module names**: PascalCase with dots — `Features.Gallery`, `Core.RPC.Types`
- **C++ files/functions**: snake_case — `gallery.ixx`, `initialize()`
- **No anonymous namespaces**: Do not use `namespace { ... }` in C++; put helpers in the module's named namespace.
- **Frontend components**: PascalCase — `GalleryPage.vue`
- **Frontend modules**: camelCase — `galleryApi.ts`
- **Module import order** in `.ixx`: `std` → `Vendor.*` → `Core.*` → `Features.*` / `UI.*` / `Utils.*`

## Testing
No automated test suite. Manual testing only:
1. Build and run the exe.
2. Use the `web/src/features/playground/` pages for interactive RPC endpoint testing during development.
3. Use the root-level `playground/` scripts for backend HTTP/RPC debugging and ad-hoc experiments.

## Adding a New Feature
1. Create a directory under `src/features/<name>/` with at minimum a `.ixx` module interface and `.cpp` implementation.
2. Add a state struct in `<name>/state.ixx` and register it in `Core::State::AppState`.
3. Add RPC endpoint file under `src/core/rpc/endpoints/<name>/`, implement `register_all(state)`, and wire it in `registry.cpp`.
4. Register commands in `src/core/commands/builtin.cpp` if the feature needs hotkeys/menu entries.
5. If the feature needs initialization, add it to `Core::Initializer::initialize_application`.
6. On the web side, add a feature directory under `web/src/features/<name>/` with `api.ts`, `store/index.ts`, `types.ts`, components, and pages.
