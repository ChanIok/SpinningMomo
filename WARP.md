# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

SpinningMomo (旋转吧大喵) is a Windows desktop application for managing game screenshots, specifically targeting "Infinity Nikki" (无限暖暖). It features a modern C++23 native backend with a Vue 3 web-based UI for photo gallery management.

The project enables window manipulation for better in-game photography (portrait mode, custom aspect ratios) and supports capturing ultra-high resolution screenshots (up to 12K).

## Build Commands

### Full Release Build
```powershell
# Build both C++ backend and web frontend, copy web resources to output
xmake build-all
```

### C++ Backend Only
```powershell
# Configure for release mode
xmake config -m release
xmake build

# Or configure for debug mode
xmake config -m debug
xmake build

# Smart release build (auto-restores debug config if you're in debug mode)
xmake release
```

### Web Frontend Only
```powershell
cd web
npm install      # First time only
npm run build    # Production build
npm run dev      # Development server
```

### Generate Visual Studio Solution
```powershell
xmake project -k vsxmake2022
```

### Output Location
Built executables are located in:
- Release: `build\windows\x64\release\`
- Debug: `build\windows\x64\debug\`

## Development Commands

### Database Migrations
```powershell
# Generate migration code from schema files
node scripts/generate-migrations.js
```

### Localization
```powershell
# Generate embedded locale modules from JSON files
node scripts/generate-embedded-locales.js
```

### Web Development
```powershell
cd web
npm run dev       # Development server with hot reload
npm run preview   # Preview production build
```

## Code Architecture

### Design Philosophy

**Critical: The C++ backend does NOT use Object-Oriented Programming (OOP).** Instead, it follows these patterns:

1. **POD Structs + Free Functions**: Plain Old Data structures with free functions operating on them
2. **Centralized State Management**: All application state lives in `AppState` (similar to a global Redux store)
3. **C++23 Modules**: Uses `.ixx` interface files and `.cpp` implementation files
4. **Functional Style**: Functions are organized by feature/domain, not by class hierarchies

### Project Structure

```
src/
├── main.cpp                    # Entry point (wWinMain)
├── app.{ixx,cpp}              # Application lifecycle
├── version.hpp                # Version constants
│
├── core/                      # Core infrastructure (technology layer)
│   ├── state/                 # Central AppState definition
│   ├── events/                # Event bus (type-safe, template-based)
│   ├── commands/              # Command registry (unified action dispatcher)
│   ├── rpc/                   # JSON-RPC bridge for frontend communication
│   ├── http_server/           # HTTP server (uWebSockets) + SSE support
│   ├── webview/               # WebView2 integration + drag handling
│   ├── database/              # SQLite database + data mapper pattern
│   ├── migration/             # Database schema migrations
│   ├── i18n/                  # Internationalization system
│   ├── async/                 # Async utilities with asio
│   ├── worker_pool/           # Thread pool for background tasks
│   ├── initializer/           # Application initialization sequence
│   └── shutdown/              # Graceful shutdown logic
│
├── ui/                        # Native Win32 UI components
│   ├── app_window/            # Main application window (native controls)
│   ├── webview_window/        # WebView2 host window (browser container)
│   ├── tray_icon/             # System tray icon
│   └── context_menu/          # Right-click menu
│
├── plugins/                   # Game-specific adapters
│   └── infinity_nikki/        # Infinity Nikki game integration
│
├── features/                  # Business features (high cohesion)
│   ├── gallery/               # Photo gallery management
│   │   ├── gallery.ixx        # Public API
│   │   ├── scanner.ixx        # Directory scanning
│   │   ├── asset/             # Asset repository + thumbnail service
│   │   ├── folder/            # Folder management
│   │   ├── tag/               # Tagging system
│   │   └── ignore/            # .gitignore-style pattern matching
│   ├── screenshot/            # Screenshot capture & management
│   ├── letterbox/             # Window resizing/aspect ratio control
│   ├── overlay/               # Full-screen overlay rendering (Direct3D 11)
│   ├── preview/               # Preview window (DirectX capture)
│   ├── recording/             # Screen recording functionality
│   │   ├── recording.ixx      # Public API
│   │   ├── state.ixx          # Recording state
│   │   ├── types.ixx          # Recording types
│   │   ├── encoder.ixx        # Video encoding
│   │   └── usecase.ixx        # Recording use cases
│   ├── window_control/        # Window manipulation & control
│   ├── update/                # Application update system
│   ├── settings/              # Settings management + menu data
│   │   ├── settings.ixx       # Public API
│   │   ├── state.ixx          # Settings state (contains raw + computed states)
│   │   ├── types.ixx          # Settings types (layered configuration structure)
│   │   ├── compute.ixx        # Settings computation logic
│   │   ├── menu.ixx           # Menu data management (presets, functional items)
│   │   ├── events.ixx         # Settings-related events
│   │   └── migration.ixx      # Settings file migration logic
│   └── notifications/         # Windows toast notifications
├── utils/                     # Utility functions
│   ├── logger/                # spdlog wrapper
│   ├── file/                  # File operations
│   └── graphics/              # Windows Graphics Capture API helpers
│
└── vendor/                    # Third-party wrappers & type aliases
    ├── windows.ixx            # Windows API type aliases
    ├── wil.ixx, winhttp.ixx   # Windows Integration Library, WinHTTP
    ├── std.ixx, xxhash.ixx    # Standard library module, hashing utilities
    └── version.ixx, build_config.ixx, shellapi.ixx

web/                           # Vue 3 frontend
├── src/
│   ├── features/              # Feature-first architecture
│   │   ├── gallery/           # Gallery pages + components
│   │   ├── settings/          # Settings management
│   │   │   ├── api.ts        # RPC API call wrappers
│   │   │   ├── store.ts      # Pinia state management
│   │   │   ├── types.ts      # TypeScript type definitions
│   │   │   ├── constants.ts  # Constant definitions
│   │   │   ├── pages/        # Page components
│   │   │   ├── components/   # Functional components (appearance, functionality, menus, etc.)
│   │   │   └── composables/  # Composable functions (theme, operation logic, etc.)
│   │   ├── home/              # Home page
│   │   ├── about/             # About page
│   │   └── playground/        # [Dev] API testing & integration test UI
│   ├── components/            # Global components (shadcn-vue)
│   ├── composables/           # Global composables (useI18n, useRpc, useToast)
│   ├── core/                  # Core infrastructure
│   │   ├── rpc/              # RPC client (transport: HTTP & WebView2)
│   │   ├── i18n/             # Internationalization
│   │   └── env/              # Environment configuration
│   ├── store/                 # Pinia global state
│   └── router/                # Vue Router configuration
└── VUE_ARCHITECTURE.md        # Detailed frontend architecture docs
```

### Key Architectural Patterns

#### 1. Centralized State Management
All application state is stored in `AppState` (defined in `src/core/state/app_state.ixx`). Every subsystem has its own state struct within `AppState`:

```cpp
struct AppState {
  std::unique_ptr<RpcState> rpc;
  std::unique_ptr<EventsState> events;
  std::unique_ptr<CommandRegistry> command_registry;
  std::unique_ptr<WebViewState> webview;
  std::unique_ptr<GalleryState> gallery;
  std::unique_ptr<OverlayState> overlay;
  std::unique_ptr<RecordingState> recording;
  std::unique_ptr<SettingsState> settings;
  // ... etc
};
```

Functions receive `AppState&` as their first parameter and access the state they need.

#### 2. Event-Driven Architecture
The event bus (`src/core/events/`) uses type-safe templates:

```cpp
// Subscribe to an event
Events::subscribe<WindowResizeEvent>(state.events, [](const WindowResizeEvent& e) {
  // handle event
});

// Send event synchronously
Events::send(*state.events, WindowResizeEvent{...});

// Post event asynchronously
Events::post(*state.events, WindowResizeEvent{...});
```

#### 3. Command System (Unified Action Dispatcher)
The command system (`src/core/commands/`) provides a unified way to invoke actions from different input sources (hotkeys, menus, RPC, etc.):

- **Command Registry**: Central registry mapping command IDs to actions and metadata
- **Builtin Commands**: Registered in `builtin.cpp` (e.g., `app.main`, `float.toggle`, `screenshot.capture`)
- **Command Invocation**: All inputs (hotkeys, context menus, RPC) invoke commands through the registry

```cpp
// Register a command
Core::Commands::register_command(registry, {
  .id = "feature.action",
  .i18n_key = "menu.feature_action",
  .is_toggle = true,
  .action = [&state]() { /* implementation */ },
  .get_state = [&state]() -> bool { return state.feature->enabled; }
});

// Invoke a command
Core::Commands::invoke_command(*state.command_registry, "feature.action");
```

#### 4. RPC Communication (Backend ↔ Frontend)
The application uses a **transport-agnostic RPC system** that works in both WebView2 (embedded) and browser (standalone) environments:

- **Backend**: HTTP server on `localhost:51206` with JSON-RPC endpoints
- **Frontend RPC Layer**: `core/rpc/` automatically selects transport based on environment:
  - **WebView2 mode**: Direct message passing via `window.chrome.webview.postMessage` (bidirectional)
  - **Browser mode**: HTTP fetch to `/rpc` endpoint + Server-Sent Events (`/sse`) for backend-to-frontend notifications
- **Environment Detection**: `core/env/` detects runtime environment (`webview` vs `web`) and configures transport accordingly

**Backend RPC Registration:**
```cpp
Core::RPC::register_method<RequestType, ResponseType>(
  app_state,
  registry,
  "method.name",
  async_handler_function,
  "Method description"
);
```

**WebView2 Integration Points:**
- **`core/webview/`**: WebView2 control lifecycle, RPC message bridge, and event handling
- **`ui/webview_window/`**: Native window that hosts the WebView2 control

#### 5. Module System (C++23)
- **Interface files** (`.ixx`): Export public API using `export module`
- **Implementation files** (`.cpp`): Import the module and implement functions
- **Import syntax**: `import ModuleName;` instead of `#include`

Example:
```cpp
// features/gallery/gallery.ixx
export module Features.Gallery;
export auto initialize(AppState& state) -> std::expected<void, std::string>;

// features/gallery/gallery.cpp
module Features.Gallery;
auto Features::Gallery::initialize(AppState& state) -> std::expected<void, std::string> {
  // implementation
}
```

#### 6. Feature Independence
Each feature in `src/features/` is self-contained with:
- Public API (`.ixx` file)
- Internal implementation (`.cpp` files)
- State structure (defined in `state.ixx`)
- Types (defined in `types.ixx`)

Features depend on `core/` but not on each other.

### Technology Stack

**Backend:**
- C++23 with modules
- xmake build system
- WebView2 for UI rendering
- Direct3D 11 for overlay/preview rendering
- Windows Graphics Capture API
- uWebSockets for HTTP/WebSocket
- SQLite (via SQLiteCpp) for database
- spdlog for logging
- asio for async I/O

**Frontend:**
- Vue 3.5 (with Vapor mode planned)
- TypeScript 5.9+
- Vite 7 with Rolldown
- Pinia for state management
- Vue Router 4
- shadcn-vue + Tailwind CSS 4
- Reka UI (headless components)

## Important Coding Patterns

### Error Handling
Use `std::expected<T, std::string>` for operations that can fail:

```cpp
auto some_function() -> std::expected<ResultType, std::string> {
  if (error_condition) {
    return std::unexpected("Error message");
  }
  return ResultType{...};
}
```

### Logging
Use the global `Logger()` function (from `Utils.Logger` module):

```cpp
Logger().info("Info message");
Logger().warn("Warning: {}", variable);
Logger().error("Error occurred: {}", error_msg);
Logger().critical("Critical failure");
```

### Async Operations
Use `asio::awaitable<T>` for async functions:

```cpp
auto async_operation(AppState& state) -> asio::awaitable<std::string> {
  auto result = co_await some_async_call();
  co_return result;
}
```

### Database Access
Use the data mapper pattern. Define your data structures in `types.ixx` and repositories in `repository.ixx`:

```cpp
// In repository.ixx
export auto find_by_id(DatabaseState& db, int64_t id) 
  -> std::expected<EntityType, std::string>;

export auto insert(DatabaseState& db, const EntityType& entity)
  -> std::expected<int64_t, std::string>;
```

### String Handling
Use `std::string` and `std::wstring`. Convert between them using utilities in `utils/`:
- UTF-8 for internal processing
- UTF-16 (wstring) for Windows API calls

## Common Development Workflows

### Adding a New Feature

1. **Create feature directory** under `src/features/your_feature/`
2. **Define state struct** in `state.ixx`:
   ```cpp
   export module Features.YourFeature.State;
   namespace Features::YourFeature::State {
     export struct YourFeatureState {
       // state fields
     };
   }
   ```
3. **Add state to AppState** in `src/core/state/app_state.ixx`
4. **Create public API** in `your_feature.ixx`:
   ```cpp
   export module Features.YourFeature;
   export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;
   ```
5. **Implement in** `your_feature.cpp`
6. **Register commands** in `src/core/commands/builtin.cpp` if the feature should be invokable via hotkeys/menus/RPC
7. **Add RPC endpoints** if frontend needs to call this feature
8. **Register event handlers** if it needs to respond to events

### Adding an RPC Endpoint

1. **Define request/response types** in feature's `types.ixx`
2. **Implement handler** in feature's `repository.ixx` or `service.ixx`
3. **Register in** `src/core/rpc/endpoints/` corresponding file:
   ```cpp
   Core::RPC::register_method<RequestType, ResponseType>(
     state, registry, "feature.methodName", handler_function
   );
   ```

### Modifying the Database Schema

1. **Edit SQL in** `src/migrations/` (numbered files like `001_initial_schema.sql`)
2. **Run migration generator**:
   ```powershell
   node scripts/generate-migrations.js
   ```
3. This generates C++ code in `src/core/migration/generated/`

### Adding UI Features (Frontend)

1. **Create feature under** `web/src/features/your_feature/`
2. Follow the structure in `web/VUE_ARCHITECTURE.md`:
   - `pages/` - Route components
   - `components/` - Feature-specific components
   - `composables/` - Feature logic
   - `api.ts` - RPC calls to backend
   - `store.ts` - Pinia store (if needed)
   - `types.ts` - TypeScript types
   - `routes.ts` - Vue Router routes
3. **Register routes** in `web/src/router/index.ts`

## Platform Specifics

### Windows Development
- Uses PowerShell 5.1+ (default on Windows)
- Requires Visual Studio 2022 with C++ Desktop Development workload
- Windows SDK 10.0.22621.0+ (Windows 11 SDK) required
- Administrator privileges needed at runtime for window manipulation

### Build System (xmake)
- Configuration file: `xmake.lua`
- Custom tasks: `tasks/*.lua`
- Dependencies managed via vcpkg integration
- Lock file: `xmake-requires.lock`

## Testing & Debugging

There are no automated tests currently. Testing is primarily manual:
1. Build the application with `xmake build`
2. Run `build\windows\x64\debug\SpinningMomo.exe`
3. Check logs in the application's log directory
4. Use Visual Studio debugger if needed

For web frontend:
1. Run `npm run dev` in `web/` directory
2. Frontend dev server will proxy RPC calls to the backend

### Frontend Development Tools
The `web/src/features/playground/` feature provides an RPC method explorer and integration testing UI. Access it during development to test backend endpoints interactively.

## Dependencies & Package Management

### C++ Dependencies (via vcpkg/xmake)
All managed in `xmake.lua`:
- uwebsockets - HTTP server
- spdlog - Logging
- asio - Async I/O
- reflectcpp - JSON serialization
- webview2 - Browser engine
- wil - Windows Implementation Library
- sqlitecpp - SQLite wrapper
- libwebp - Image format support

### Web Dependencies (via npm)
Managed in `web/package.json`:
- Core: vue, pinia, vue-router
- UI: reka-ui, tailwindcss, lucide-vue-next
- Build: vite (rolldown-vite fork), vue-tsc

## Additional Notes

### Game Adapters (Plugins)
The `src/plugins/` directory contains game-specific integration modules (currently only `infinity_nikki`). These adapters expose game-specific RPC endpoints via `core/rpc/endpoints/plugins/` and provide tailored functionality for individual games.

### Code Generation
Two scripts generate code:
- `scripts/generate-migrations.js` - Database migrations
- `scripts/generate-embedded-locales.js` - Locale strings

Always run these after modifying SQL schemas or locale JSON files.

### Naming Conventions
- **C++ modules**: PascalCase with dots (e.g., `Features.Gallery`)
- **C++ files**: snake_case (e.g., `gallery.ixx`)
- **C++ functions**: snake_case (e.g., `initialize()`)
- **Frontend files**: PascalCase for components (e.g., `GalleryPage.vue`), camelCase for modules (e.g., `galleryApi.ts`)

### Module Import Order
In `.ixx` files, import in this order:
1. `std` (standard library)
2. Vendor/third-party modules
3. Core modules
4. Feature modules