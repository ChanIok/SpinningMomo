# Architecture

> [!NOTE]
> This project recommends discussing requirements and direction through an issue before submitting a pull request.
>
> For new features, behavior changes, or larger refactors, please open an issue first and describe the problem being solved, the use case, and the expected outcome. This helps confirm whether the change fits the project direction before implementation begins.
>
> Pull requests are very welcome for issues with confirmed scope, clear bug fixes, documentation improvements, and technical challenges that have already been discussed. Unsolicited feature PRs may not be merged if they do not align with the project direction.

This project uses a hybrid architecture with a **C++23 native backend** and a **Vue 3 web frontend**. The backend uses self-contained `.hpp + .cpp` sources with a PCH used only for build acceleration. Project code includes external headers through exact facades under `src/vendor/`; Windows SDK facades map one-to-one to physical headers so domain aggregates do not couple unrelated call sites to the PCH. For the full design philosophy, component breakdown, and dependency graph, check the root-level **[`AGENTS.md`](https://github.com/ChanIok/SpinningMomo/blob/main/AGENTS.md)**.

## Prerequisites

The C++ backend defaults to `clang-cl[llvm]` (Clang + LLD) for daily development. Release builds use MSVC.

| Tool | Requirement | Notes |
|------|-------------|-------|
| **Visual Studio 2026 / LLVM** | Includes C++ and Clang (`clang-cl`) toolchains | |
| **Windows SDK** | 10.0.22621.0+ (Windows 11 SDK) | |
| **Git** | Latest | Clone vcpkg and fetch third-party dependencies |
| **xmake** | 3.1.0 | C++ build system |
| **Node.js** | v20+ | Web frontend build and npm scripts |

### Install xmake

```powershell
# PowerShell (recommended)
irm https://xmake.io/psget.text | iex

# Or download from the official site
# https://xmake.io/#/getting_started?id=installation
```

### Set up vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\dev\vcpkg  # path is up to you
cd D:\dev\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
```

---

## Dependency Setup

### 1. Third-party dependencies

```bash
npm run fetch:third-party
```

### 2. npm dependencies

```bash
# Root (build script deps)
npm install

# Web frontend
npm ci --prefix web
```

### 3. Initialize xmake dependencies and apply patches

```bash
node scripts/patch-xmake-clang-cl-cxx23.js

# Clang-cl + LLD (default)
xmake f --toolchain="clang-cl[llvm]" -y

# Or use MSVC
# xmake f --toolchain=msvc -y

xmake f -m release -y && xmake f -m debug -y
node scripts/patch-vcpkg.js
```

---

## Visual Studio Development (Optional)

To browse, edit, and debug the C++ code in Visual Studio, generate an
Xmake-managed solution:

```powershell
xmake vs
```

Then open:

```text
vsxmake2026\SpinningMomo.sln
```

---

## Build

> [!TIP]
> If you encounter environment, dependency, or toolchain issues during local setup, you can refer to the [Build Release Workflow](https://github.com/ChanIok/SpinningMomo/blob/main/.github/workflows/build-release.yml) for an up-to-date, automated reference build procedure.

### Full Build (Recommended)

```bash
# One command: C++ Release + Web frontend + assemble dist/
npm run build
```

Output goes to `dist/`.

### Step-by-Step

```bash
# C++ backend — Debug (daily development)
xmake config -m debug
xmake build

# C++ backend — Release
xmake release    # automatically restores debug config after release build

# Web frontend
npm run build --prefix web

# Assemble dist/ (exe + web resources)
npm run build:dist
```

### Build Output Paths

| Type | Path |
|------|------|
| Debug | `build\windows\x64\debug\` |
| Release | `build\windows\x64\release\` |
| Packaged | `dist\` |

---

## Packaging

### Portable (ZIP)

```bash
npm run build:portable
```

### MSI Installer

Requires WiX Toolset v6:

```bash
dotnet tool install --global wix --version 6.0.2
wix extension add WixToolset.UI.wixext/6.0.2 --global
wix extension add WixToolset.BootstrapperApplications.wixext/6.0.2 --global
```

Then run:

```bash
npm run build:installer
```

---

## Web Frontend Development

Start the dev server (C++ backend needs to be running):

```bash
npm run dev:web
```

Vite dev server proxies `/rpc` and `/static` to the C++ backend (`localhost:51206`).

---

## Code Generation Scripts

Re-run these when their source files change:

| What changed | Run this script |
|--------------|-----------------|
| `src/migrations/*.sql` | `node scripts/generate-migrations.js` |
| `src/locales/*.json` | `node scripts/generate-embedded-locales.js` |
