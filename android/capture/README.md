# Android 屏幕采集服务 (momo-capture)

轻量级 Android 端屏幕捕获服务。通过 ADB 推送到设备临时目录并由 `app_process` 启动（Shell UID: 2000），无需安装 APK 或前台授权弹窗。

---

## 架构原理

```text
app_process (Shell UID)
  ├─ Workarounds / FakeContext   -> 伪造 Looper、ActivityThread 与 Context，保证系统服务调用正常
  ├─ DisplayManagerGlobal        -> 反射获取主屏原生尺寸与配置
  └─ VirtualDisplay (镜像模式)
       ├─ 【当前：截图】绑定 ImageReader -> 内存直取最新帧 -> Bitmap 压缩为 JPEG/PNG
       └─ 【规划：录制】绑定 MediaCodec  -> 硬件编码器输入 Surface -> 零拷贝推流 H.264
```

* **常驻会话**：通过 Unix Abstract Socket 与 Windows 端保持单一稳定连接，避免频繁冷启动 `app_process`。
* **采集模型**：彻底废除脆弱的 `SurfaceControl` 反射快照，统一基于 `VirtualDisplay`，为后续视频录制奠定完全复用的硬件底座。

---

## 开发与构建

仅需 JDK 与 Android 命令行工具，无需 Android Studio、Gradle 或 NDK。

### 环境依赖
* **JDK 21**：设置环境变量 `JAVA_HOME`
* **Android SDK**：设置环境变量 `ANDROID_HOME`（需安装 `platforms;android-36` 与 `build-tools;36.0.0`）

### 构建命令
```powershell
pnpm run build:android
# 或直接运行：node scripts/build-android.js
```
构建产物输出至 `build/android/momo-capture.jar`（约 10 KB）。

---

## 独立调试与测试

脱离 Windows C++ 主程序直接验证模拟器/真机连通性：

```powershell
# 1. 采集设备与编码器能力报告 (JSON)
node scripts/run-android.js --adb "C:\路径\adb.exe" --serial "127.0.0.1:16384"

# 2. 独立抓取一帧 JPEG 截图到本地
node scripts/run-android.js --adb "C:\路径\adb.exe" --serial "127.0.0.1:16384" --screenshot --output "build/screenshot.jpg"
```
