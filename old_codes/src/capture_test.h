#pragma once

#include <windows.h>
#include <string>

bool CaptureWindowToFile(HWND window, const std::wstring& filename);
void CaptureTest(); 