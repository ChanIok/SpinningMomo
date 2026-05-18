# Architecture

> [!NOTE]
> This project recommends discussing requirements and direction through an issue before submitting a pull request.
>
> For new features, behavior changes, or larger refactors, please open an issue first and describe the problem being solved, the use case, and the expected outcome. This helps confirm whether the change fits the project direction before implementation begins.
>
> Pull requests are very welcome for issues with confirmed scope, clear bug fixes, documentation improvements, and technical challenges that have already been discussed. Unsolicited feature PRs may not be merged if they do not align with the project direction.

This project uses a hybrid architecture with a **C++23 native backend** and a **Vue 3 web frontend**. For the full design philosophy, module breakdown, and dependency graph, check the root-level **[`AGENTS.md`](https://github.com/ChanIok/SpinningMomo/blob/main/AGENTS.md)**.

## Prerequisites

| Tool | Requirement | Notes |
|------|-------------|-------|
| **Visual Studio 2022+** | "Desktop development with C++" workload | Also check "**C++ Modules for v143 (with the standard library)**" |
| **Windows SDK** | 10.0.22621.0+ (Windows 11 SDK) | |
| **xmake** | Latest | C++ build system, manages vcpkg dependencies |
| **Node.js** | v20+ | Web frontend build and npm scripts |

### Install xmake

```powershell
# PowerShell (recommended)
iwr -useb https://xmake.io/psget.txt | iex

# Or download from the official site
# https://xmake.io/#/getting_started?id=installation
```

> xmake automatically pulls C++ dependencies via vcpkg using `xmake-requires.lock` — **no manual vcpkg setup needed**.

---

## Dependency Setup

### 1. Third-party dependencies

```powershell
.\scripts\fetch-third-party.ps1
```

### 2. npm dependencies

```bash
# Root (build script deps)
npm install

# Web frontend
cd web && npm ci
```

---

## Build

### Full Build (Recommended)

```bash
# One command: C++ Release + Web frontend + assemble dist/
npm run build:ci
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
cd web && npm run build

# Assemble dist/ (exe + web resources)
npm run build:prepare
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
