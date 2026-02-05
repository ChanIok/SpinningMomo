# AGENTS.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

SpinningMomo (旋转吧大喵) is a Windows-only desktop tool for the game "Infinity Nikki" (无限暖暖) that adjusts game window aspect ratios/resolutions for photography. It supports portrait orientation, ultra-high-resolution screenshots (up to 12K), overlay rendering, and a gallery viewer. The codebase is bilingual — code comments and UI strings are predominantly in Chinese.

## Build & Development

### Prerequisites
- Visual Studio 2022+ with Windows SDK 10.0.22621.0+
- xmake (primary build system)
- Node.js / npm (for web frontend and formatting scripts)

### Build Commands
```
# C++ backend — debug (default)
xmake config -m debug
xmake build

# C++ backend — release
xmake release          # builds release then restores debug config

# Web frontend (Vue 3 + Vite, in web/ directory)
cd web && npm run build

# Full build: C++ release + web + assemble dist/
npm run build

# Generate VS2022 solution (debug+release)
xmake vs
```

### Formatting
```
# C++ — clang-format (Google style, 100-col limit)
npm run format:cpp

# Web — Prettier
npm run format:web
```

A husky pre-commit hook runs `lint-staged` which auto-formats staged C++ (`.cpp`, `.ixx`, `.h`, `.hpp`) and web files.

### Web Frontend Dev Server
```
cd web && npm run dev
```
Vite dev server proxies `/rpc` and `/static` to the C++ backend at `localhost:51206`.

## Architecture

### Two-Process Model
The application is a **native Win32 C++ backend** that hosts an embedded **WebView2** frontend. Communication happens over **JSON-RPC 2.0** through two transport layers:
- **WebView bridge** — used when the Vue app runs inside WebView2 (production)
- **HTTP + SSE** — used when the Vue app runs in a browser during development (uWebSockets on port 51206). SSE provides server-to-client push notifications.

The frontend auto-detects its environment (`window.chrome.webview` presence) and selects the appropriate transport.

### C++ Module System
The backend uses **C++23 modules** (`.ixx` interface files, `.cpp` implementation files). Module names follow a dotted hierarchy that mirrors the directory structure:

- `Core.*` — framework infrastructure (async runtime, database, events, HTTP server, RPC, WebView, i18n, commands, migrations, worker pool)
- `Features.*` — business logic (gallery, letterbox, overlay, preview, recording, screenshot, settings, update, window_control, notifications)
- `UI.*` — native Win32 UI (floating_window, tray_icon, context_menu, webview_window)
- `Utils.*` — shared utilities (logger, file, graphics, image, path, string, system, throttle, timer, dialog, lru_cache, time)
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
Vue 3 + TypeScript + Pinia + Tailwind CSS v4 + shadcn-vue (reka-ui based). Key directories:
- `web/src/core/rpc/` — JSON-RPC client with WebView and HTTP transports
- `web/src/core/i18n/` — client-side i18n
- `web/src/features/` — feature modules (gallery, settings, home, about)
- `web/src/composables/` — shared composables (`useRpc`, `useI18n`, `useToast`)

### RPC Endpoint Organization
Endpoints live under `src/core/rpc/endpoints/<domain>/`, each domain exposes a `register_all(state)` called from `registry.cpp`. Game-specific adapters in `src/plugins/` (currently `infinity_nikki`) are exposed via `rpc/endpoints/plugins/`.

### Initialization Order
`main.cpp` → `Application::Initialize()` → `Core::Initializer::initialize_application()` which runs:
events → async runtime → worker pool → RPC registry → HTTP server → database + migrations → settings → update → commands → floating window → tray icon → context menu → recording → gallery → hotkeys.

## Build Output
- Release: `build\windows\x64\release\`
- Debug: `build\windows\x64\debug\`
- Distribution: `dist/` (exe + web resources)

## Installer
MSI installer built with WiX Toolset via `scripts/build-msi.ps1`. Build artifacts are assembled into `dist/` by `scripts/prepare-dist.js`.

## Code Generation Scripts
These must be re-run when their source files change:
- `node scripts/generate-migrations.js` — after modifying `src/migrations/*.sql`
- `node scripts/generate-embedded-locales.js` — after modifying `src/locales/*.json` (zh-CN / en-US)

## Naming Conventions
- **C++ module names**: PascalCase with dots — `Features.Gallery`, `Core.RPC.Types`
- **C++ files/functions**: snake_case — `gallery.ixx`, `initialize()`
- **Frontend components**: PascalCase — `GalleryPage.vue`
- **Frontend modules**: camelCase — `galleryApi.ts`
- **Module import order** in `.ixx`: `std` → `Vendor.*` → `Core.*` → `Features.*` / `UI.*` / `Utils.*`

## Testing
No automated test suite. Manual testing only:
1. Build and run the exe.
2. Frontend has a `web/src/features/playground/` page for interactive RPC endpoint testing during development.

## Adding a New Feature
1. Create a directory under `src/features/<name>/` with at minimum a `.ixx` module interface and `.cpp` implementation.
2. Add a state struct in `<name>/state.ixx` and register it in `Core::State::AppState`.
3. Add RPC endpoint file under `src/core/rpc/endpoints/<name>/`, implement `register_all(state)`, and wire it in `registry.cpp`.
4. Register commands in `src/core/commands/builtin.cpp` if the feature needs hotkeys/menu entries.
5. If the feature needs initialization, add it to `Core::Initializer::initialize_application`.
6. On the web side, add a feature directory under `web/src/features/<name>/` with `api.ts`, `store.ts`, `types.ts`, components, and pages.
