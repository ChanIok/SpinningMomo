# 构建指南

## 环境要求

### 必需软件
- **Visual Studio 2022** - 包含"使用C++的桌面开发"工作负载，确保包含C++模块支持
- **Node.js** - 前端构建
- **Xmake** - 构建系统

### 安装 Xmake
```bash
# Windows (使用 PowerShell)
iwr -useb https://xmake.io/psget.txt | iex

# 或者下载安装包
# 访问 https://xmake.io/#/getting_started?id=installation
```

## vcpkg 配置

### 安装 vcpkg
```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
```

### 启用 vcpkg 集成
```bash
./vcpkg integrate install
```

## 构建流程

### 快速构建
```bash
xmake build-all
```

此命令会自动完成以下步骤：
1. 配置并构建release版本
2. 构建Web应用
3. 复制Web资源到输出目录

### 分步构建

#### 1. 安装前端依赖
```bash
cd web
npm install
```

#### 2. 构建项目
```bash
# 构建release版本
xmake config -m release
xmake build

# 构建web应用
cd web
npm run build

# 复制web资源
mkdir -p build/release/resources/web
cp -r web/dist/* build/release/resources/web/
```

### 构建选项说明
- **release**: 发布版本，优化性能
- **debug**: 调试版本，包含调试信息

构建完成后，可执行文件位于：
```
build/release/SpinningMomo.exe
```

---

此构建指南基于项目当前的xmake配置，如需调整构建选项请参考 `xmake.lua` 和 `tasks/build-all.lua`。