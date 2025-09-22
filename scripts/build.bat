@echo off
echo 正在设置 Visual Studio 开发环境...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64

echo 正在配置项目 (x64-Debug)...
cmake --preset=x64-Debug

if %errorlevel% neq 0 (
    echo 配置失败！错误码: %errorlevel%
    pause
    exit /b %errorlevel%
)

echo 正在构建项目...
cmake --build --preset=x64-Debug

if %errorlevel% neq 0 (
    echo 构建失败！错误码: %errorlevel%
    pause
    exit /b %errorlevel%
)

echo 构建成功完成！
pause
