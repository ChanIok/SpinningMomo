#pragma once

// Windows SDK headers - 核心重型头文件
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <dwmapi.h>

// DirectX headers - 图形相关重型头文件
#include <d2d1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi.h>

// COM and Windows Implementation Library headers - 模板重型库
#include <comdef.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>

// 其他可能的重型第三方库头文件
#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>