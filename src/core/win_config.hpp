#pragma once

// 确保Windows Unicode设置一致
#ifndef UNICODE
    #define UNICODE
#endif

#ifndef _UNICODE
    #define _UNICODE
#endif

// 确保Windows头文件最小化
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

// Windows API头文件
#include <windows.h> 