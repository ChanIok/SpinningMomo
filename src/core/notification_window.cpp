#include "notification_window.hpp"
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>

static const TCHAR* NOTIFICATION_WINDOW_CLASS = TEXT("SpinningMomoNotificationClass");

NotificationWindow::NotificationWindow(HINSTANCE hInstance) 
    : m_hInstance(hInstance) {
    // 获取系统 DPI
    HDC hdc = GetDC(NULL);
    m_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);

    UpdateDpiDependentResources();
    RegisterWindowClass();
    CreateNotificationWindow();
}

NotificationWindow::~NotificationWindow() {
    if (m_animationTimer) {
        KillTimer(m_hwnd, m_animationTimer);
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
    UnregisterClass(NOTIFICATION_WINDOW_CLASS, m_hInstance);
}

int NotificationWindow::CalculateWindowHeight(const std::wstring& message) const {
    // 创建临时DC和字体
    HDC hdc = GetDC(NULL);
    HFONT messageFont = CreateFont(-m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));
    HFONT oldFont = (HFONT)SelectObject(hdc, messageFont);
    
    // 计算文本区域宽度（窗口宽度减去左右内边距）
    int textWidth = m_windowWidth - (m_contentPadding * 2);
    
    // 计算文本实际高度
    RECT textRect = {0, 0, textWidth, 0};
    UINT format = DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS;
    DrawText(hdc, message.c_str(), -1, &textRect, format);
    int contentHeight = textRect.bottom - textRect.top;
    
    // 清理资源
    SelectObject(hdc, oldFont);
    DeleteObject(messageFont);
    ReleaseDC(NULL, hdc);
    
    // 计算总高度：标题高度 + 内容高度 + 上下内边距
    int totalHeight = m_titleHeight + contentHeight + (m_padding * 2);
    
    // 根据DPI缩放基础高度限制
    int minHeight = MulDiv(BASE_MIN_HEIGHT, m_dpi, 96);
    int maxHeight = MulDiv(BASE_MAX_HEIGHT, m_dpi, 96);
    
    // 确保高度在限制范围内
    return std::clamp(totalHeight, minHeight, maxHeight);
}

void NotificationWindow::Show(const std::wstring& title, const std::wstring& message, NotificationType type) {
    if (!m_hwnd && !CreateNotificationWindow()) {
        return;
    }

    m_currentNotification = std::make_unique<NotificationItem>();
    m_currentNotification->title = title;
    m_currentNotification->message = message;
    m_currentNotification->type = type;
    m_currentNotification->timestamp = GetTickCount64();
    
    // 根据消息内容计算窗口高度
    m_windowHeight = CalculateWindowHeight(message);
    
    m_isActive = true;
    m_animState = AnimationState::SLIDING_IN;
    
    // 先计算位置
    UpdateWindowPosition();
    
    // 更新窗口大小和位置
    SetWindowPos(m_hwnd, NULL, m_startPos.x, m_startPos.y, m_windowWidth, m_windowHeight, 
                SWP_NOZORDER | SWP_NOACTIVATE);
    
    StartAnimation();
}

void NotificationWindow::StartFadeOut() {
    if (!m_isActive) return;
    
    m_animState = AnimationState::FADING_OUT;
    m_animProgress = 0.0f;
    m_currentNotification->timestamp = GetTickCount64();
    StartAnimation();
}

void NotificationWindow::StartMoveUpAnimation(int nextWindowHeight) {
    if (!m_isActive) return;

    m_animState = AnimationState::MOVING_UP;
    m_startPos = m_targetPos;
    
    // 使用下一个窗口的实际高度计算滑动距离
    int spacing = MulDiv(BASE_SPACING, m_dpi, 96);
    m_targetPos = {
        m_targetPos.x,
        m_targetPos.y - (nextWindowHeight + spacing)
    };
    
    m_animProgress = 0.0f;
    m_currentNotification->timestamp = GetTickCount64();
    StartAnimation();
}

POINT NotificationWindow::CalculateNextPosition() const {
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    return {
        workArea.right - m_windowWidth - m_padding,
        m_targetPos.y - (m_windowHeight + BASE_SPACING)
    };
}

void NotificationWindow::OnAnimationComplete() {
    switch (m_animState) {
        case AnimationState::SLIDING_IN:
            m_animState = AnimationState::DISPLAYING;
            m_currentNotification->timestamp = GetTickCount64();  // 重置显示时间
            break;
            
        case AnimationState::MOVING_UP:
            m_animState = AnimationState::DISPLAYING;
            break;
            
        case AnimationState::FADING_OUT:
            m_isActive = false;
            ShowWindow(m_hwnd, SW_HIDE);
            if (m_onLifecycleEnd) {
                m_onLifecycleEnd(this);
            }
            break;
    }
}

void NotificationWindow::OnDisplayTimeout() {
    if (m_animState == AnimationState::DISPLAYING) {
        StartFadeOut();
    }
}

void NotificationWindow::OnTimer(UINT_PTR timerId) {
    if (!m_currentNotification) return;

    ULONGLONG currentTime = GetTickCount64();
    ULONGLONG elapsedTime;

    // 计算实际经过的时间（减去暂停时间）
    if (m_isWindowHovered && m_pausedTime > 0) {
        // 如果当前正在暂停，使用暂停开始时间计算
        elapsedTime = m_pausedTime - m_currentNotification->timestamp - m_totalPausedTime;
    } else {
        // 否则使用当前时间减去累计暂停时间
        elapsedTime = currentTime - m_currentNotification->timestamp - m_totalPausedTime;
    }

    bool shouldInvalidate = true;

    // 处理不同的动画状态
    switch (m_animState) {
        case AnimationState::SLIDING_IN: {
            m_animProgress = min(1.0f, static_cast<float>(elapsedTime) / SLIDE_DURATION);
            float easedProgress = EaseOutCubic(m_animProgress);
            
            int newX = m_startPos.x + static_cast<int>((m_targetPos.x - m_startPos.x) * easedProgress);
            int newY = m_startPos.y + static_cast<int>((m_targetPos.y - m_startPos.y) * easedProgress);
            
            SetWindowPos(m_hwnd, HWND_TOPMOST, newX, newY, 0, 0, 
                        SWP_NOSIZE | SWP_NOACTIVATE);

            m_currentNotification->opacity = easedProgress;

            if (m_animProgress >= 1.0f) {
                OnAnimationComplete();
            }
            break;
        }

        case AnimationState::MOVING_UP: {
            m_animProgress = min(1.0f, static_cast<float>(elapsedTime) / SLIDE_DURATION);
            float easedProgress = EaseOutCubic(m_animProgress);
            
            int newX = m_startPos.x + static_cast<int>((m_targetPos.x - m_startPos.x) * easedProgress);
            int newY = m_startPos.y + static_cast<int>((m_targetPos.y - m_startPos.y) * easedProgress);
            
            SetWindowPos(m_hwnd, HWND_TOPMOST, newX, newY, 0, 0, 
                        SWP_NOSIZE | SWP_NOACTIVATE);

            if (m_animProgress >= 1.0f) {
                OnAnimationComplete();
            }
            break;
        }
        
        case AnimationState::DISPLAYING: {
            // 只有在不处于悬停状态时才检查超时
            if (!m_isWindowHovered && elapsedTime >= DISPLAY_DURATION) {
                OnDisplayTimeout();
            }
            shouldInvalidate = false;
            break;
        }

        case AnimationState::FADING_OUT: {
            m_animProgress = min(1.0f, static_cast<float>(elapsedTime) / ANIMATION_DURATION);
            m_currentNotification->opacity = 1.0f - EaseOutCubic(m_animProgress);
            
            if (m_animProgress >= 1.0f) {
                OnAnimationComplete();
            }
            break;
        }
    }

    if (shouldInvalidate) {
        InvalidateRect(m_hwnd, NULL, TRUE);
    }
}

void NotificationWindow::RegisterWindowClass() {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = NotificationWindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = NOTIFICATION_WINDOW_CLASS;
    wc.hbrBackground = NULL;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);
}

bool NotificationWindow::CreateNotificationWindow() {
    m_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        NOTIFICATION_WINDOW_CLASS,
        TEXT("Notification"),
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, m_windowWidth, m_windowHeight,
        NULL, NULL, m_hInstance, this
    );

    if (!m_hwnd) return false;

    // 设置窗口圆角和阴影
    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    
    DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(m_hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
    
    BOOL value = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_ALLOW_NCPAINT, &value, sizeof(value));
    
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    return true;
}

void NotificationWindow::UpdateDpiDependentResources() {
    double scale = static_cast<double>(m_dpi) / 96.0;
    m_windowWidth = static_cast<int>(BASE_WINDOW_WIDTH * scale);
    m_windowHeight = static_cast<int>(BASE_WINDOW_HEIGHT * scale);
    m_padding = static_cast<int>(BASE_PADDING * scale);
    m_titleHeight = static_cast<int>(BASE_TITLE_HEIGHT * scale);
    m_fontSize = static_cast<int>(BASE_FONT_SIZE * scale);
    m_titleFontSize = static_cast<int>(BASE_TITLE_FONT_SIZE * scale);
    m_closeSize = static_cast<int>(BASE_CLOSE_SIZE * scale);
    m_closePadding = static_cast<int>(BASE_CLOSE_PADDING * scale);
    m_contentPadding = static_cast<int>(BASE_CONTENT_PADDING * scale);
}

void NotificationWindow::StartAnimation() {
    if (m_animationTimer) {
        KillTimer(m_hwnd, m_animationTimer);
    }
    m_animationTimer = SetTimer(m_hwnd, 1, 16, NULL);  // ~60fps
    ShowWindow(m_hwnd, SW_SHOWNA);
    UpdateWindow(m_hwnd);
}

void NotificationWindow::UpdateWindowPosition() {
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int baseX = workArea.right - m_windowWidth - m_padding;
    
    // 新通知总是从屏幕底部滑入
    m_startPos = {baseX, workArea.bottom};
    m_targetPos = {baseX, workArea.bottom - m_windowHeight - m_padding};
}

float NotificationWindow::EaseOutCubic(float t) const {
    float ft = 1.0f - t;
    return 1.0f - ft * ft * ft;
}

LRESULT CALLBACK NotificationWindow::NotificationWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NotificationWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<NotificationWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<NotificationWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT NotificationWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            OnPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }

        case WM_MOUSELEAVE: {
            OnMouseLeave();
            return 0;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            // 如果点击了关闭按钮或其他区域
            if (IsPointInCloseButton(x, y) || true) {
                StartFadeOut();
            }
            return 0;
        }

        case WM_TIMER: {
            OnTimer(wParam);
            return 0;
        }

        case WM_DESTROY: {
            if (m_animationTimer) {
                KillTimer(hwnd, m_animationTimer);
                m_animationTimer = 0;
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void NotificationWindow::OnPaint(HDC hdc) {
    if (!m_currentNotification) return;

    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // 创建双缓冲
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // 设置文本渲染质量
    SetBkMode(memDC, TRANSPARENT);
    SetTextCharacterExtra(memDC, 0);
    
    // 创建字体
    HFONT titleFont = CreateFont(-m_titleFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));
    HFONT messageFont = CreateFont(-m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));

    // 绘制背景
    HBRUSH hBackBrush = CreateSolidBrush(BG_COLOR);
    FillRect(memDC, &rect, hBackBrush);
    DeleteObject(hBackBrush);

    // 计算文本区域
    int textLeft = m_contentPadding;
    int textRight = rect.right - m_closePadding;

    // 绘制标题
    SelectObject(memDC, titleFont);
    SetTextColor(memDC, TITLE_COLOR);
    RECT titleRect = {textLeft, m_padding, textRight, m_padding + m_titleHeight};
    DrawText(memDC, m_currentNotification->title.c_str(), -1, &titleRect, 
            DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

    // 绘制消息
    SelectObject(memDC, messageFont);
    SetTextColor(memDC, TEXT_COLOR);
    RECT messageRect = {textLeft, m_padding + m_titleHeight, 
                       textRight, rect.bottom - m_padding};
    // 使用与计算高度时完全相同的参数
    UINT format = DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS;
    DrawText(memDC, m_currentNotification->message.c_str(), -1, &messageRect, format);

    // 绘制关闭按钮
    DrawCloseButton(memDC, rect);

    // 设置窗口透明度
    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(m_currentNotification->opacity * 255);
    blend.AlphaFormat = 0;

    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {rect.right, rect.bottom};
    UpdateLayeredWindow(m_hwnd, hdc, NULL, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    // 清理资源
    SelectObject(memDC, oldBitmap);
    DeleteObject(titleFont);
    DeleteObject(messageFont);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

void NotificationWindow::OnMouseMove(int x, int y) {
    bool wasHovered = m_isCloseHovered;
    m_isCloseHovered = IsPointInCloseButton(x, y);
    
    // 更新窗口悬停状态
    if (!m_isWindowHovered) {
        m_isWindowHovered = true;
        // 记录暂停开始时间
        if (m_animState == AnimationState::DISPLAYING) {
            m_pausedTime = GetTickCount64();
        }
    }

    if (wasHovered != m_isCloseHovered) {
        InvalidateRect(m_hwnd, NULL, TRUE);
    }

    // 确保能收到 WM_MOUSELEAVE 消息
    TRACKMOUSEEVENT tme = {0};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hwnd;
    TrackMouseEvent(&tme);
}

void NotificationWindow::OnMouseLeave() {
    if (m_isCloseHovered) {
        m_isCloseHovered = false;
        InvalidateRect(m_hwnd, NULL, TRUE);
    }

    // 更新窗口悬停状态
    if (m_isWindowHovered) {
        m_isWindowHovered = false;
        // 计算累计暂停时间
        if (m_pausedTime > 0) {
            m_totalPausedTime += GetTickCount64() - m_pausedTime;
            m_pausedTime = 0;
        }
    }
}

bool NotificationWindow::IsPointInCloseButton(int x, int y) const {
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    
    int closeX = rect.right - m_closeSize - m_closePadding;
    int closeY = m_padding;
    
    return x >= closeX && x <= closeX + m_closeSize &&
           y >= closeY && y <= closeY + m_closeSize;
}

void NotificationWindow::DrawCloseButton(HDC hdc, const RECT& rect) {
    int x = rect.right - m_closeSize - m_closePadding;
    int y = m_padding;

    // 设置高质量绘图模式
    SetGraphicsMode(hdc, GM_ADVANCED);
    SetBkMode(hdc, TRANSPARENT);
    SetROP2(hdc, R2_COPYPEN);

    // 根据DPI调整线条粗细
    int penWidth = max(2, m_dpi / 48);  // 96 DPI时为2px，144 DPI时为3px，192 DPI时为4px

    // 创建画笔
    COLORREF closeColor = m_isCloseHovered ? CLOSE_HOVER_COLOR : CLOSE_NORMAL_COLOR;
    LOGBRUSH lb = {BS_SOLID, closeColor, 0};
    HPEN hClosePen = ExtCreatePen(
        PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
        penWidth,
        &lb,
        0,
        nullptr
    );
    HGDIOBJ oldPen = SelectObject(hdc, hClosePen);

    // 计算内缩边距，使X符号居中且大小适中
    int padding = penWidth + 1;
    int effectiveSize = m_closeSize - (padding * 2);

    // 绘制"×"符号
    MoveToEx(hdc, x + padding, y + padding, NULL);
    LineTo(hdc, x + padding + effectiveSize, y + padding + effectiveSize);
    MoveToEx(hdc, x + padding + effectiveSize, y + padding, NULL);
    LineTo(hdc, x + padding, y + padding + effectiveSize);

    // 清理资源
    SelectObject(hdc, oldPen);
    DeleteObject(hClosePen);
}
