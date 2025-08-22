# 构建指南

## 环境要求

### 必需软件
- **Visual Studio 2022** - 包含"使用C++的桌面开发"工作负载，确保包含C++模块支持
- **Node.js** - 前端构建

### 环境配置
- **Ninja路径**: 将 `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja` 添加到PATH

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

### 1. 构建前端
```bash
cd web
npm i
npm run build
```

### 2. 构建后端
- 在 Visual Studio 2022 中打开 CMakeLists.txt
- 选择预设配置 (x64-Debug 或 x64-Release)
- 点击"生成" → "全部生成"

### 构建选项说明
- **x64-Debug**: 调试版本，动态链接运行时库
- **x64-Release**: 发布版本，静态链接运行时库

构建完成后，可执行文件位于：
```
out/build/x64-Release/SpinningMomo.exe
```

---

此构建指南基于项目当前的CMake配置和vcpkg依赖管理，如需调整构建选项请参考 `CMakeLists.txt` 和 `CMakePresets.json`。