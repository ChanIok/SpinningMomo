module;

#include <wil/com.h>
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>  // 必须放最后面

module Core.WebView.DragHandler;

import std;
import Core.State;
import Core.WebView.State;
import Utils.Logger;
import Vendor.Windows;

// COM对象实现，用于处理前端拖动请求
class DragHandlerHostObject
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDispatch> {
 private:
  Core::State::AppState* m_state;

 public:
  DragHandlerHostObject(Core::State::AppState* state) : m_state(state) {}

  // IDispatch接口实现
  STDMETHOD(GetTypeInfoCount)(UINT* pctinfo) override {
    *pctinfo = 0;
    return S_OK;
  }

  STDMETHOD(GetTypeInfo)(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override {
    if (iTInfo != 0) {
      return DISP_E_BADINDEX;
    }
    *ppTInfo = nullptr;
    return E_NOTIMPL;
  }

  STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid,
                           DISPID* rgDispId) override {
    // 检查 riid 是否为 IID_NULL
    if (riid != IID_NULL) {
      return DISP_E_UNKNOWNINTERFACE;
    }

    // 处理每个名字
    for (UINT i = 0; i < cNames; i++) {
      if (wcscmp(rgszNames[i], L"mouseDownDrag") == 0) {
        rgDispId[i] = 1;
      } else {
        rgDispId[i] = DISPID_UNKNOWN;
        return DISP_E_UNKNOWNNAME;
      }
    }

    return S_OK;
  }

  STDMETHOD(Invoke)(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                    DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
                    UINT* puArgErr) override {
    // 检查 riid 是否为 IID_NULL
    if (riid != IID_NULL) {
      return DISP_E_UNKNOWNINTERFACE;
    }

    // 检查调用标志是否为方法调用
    if (wFlags != DISPATCH_METHOD) {
      return DISP_E_BADPARAMCOUNT;
    }

    // 处理 mouseDownDrag 方法 (DISPID 1)
    if (dispIdMember == 1) {
      // 检查参数数量（应该没有参数）
      if (pDispParams && pDispParams->cArgs > 0) {
        return DISP_E_BADPARAMCOUNT;
      }

      // 释放鼠标捕获
      ReleaseCapture();
      // 发送消息开始拖动窗口
      SendMessage(m_state->webview->window.webview_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      Logger().info("Drag handler triggered");

      return S_OK;
    }

    return DISP_E_MEMBERNOTFOUND;
  }
};

namespace Core::WebView::DragHandler {
auto register_drag_handler(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.webview) {
    return std::unexpected("WebView state not initialized");
  }

  auto& webview_state = *state.webview;

  try {
    // 创建拖动处理对象
    auto drag_handler = Microsoft::WRL::Make<DragHandlerHostObject>(&state);

    // 添加到WebView脚本对象
    wil::com_ptr<ICoreWebView2> webview2;
    webview_state.resources.webview->QueryInterface(IID_PPV_ARGS(&webview2));

    if (webview2) {
      // 将COM对象包装为VARIANT类型
      VARIANT drag_handler_variant = {};
      drag_handler_variant.vt = VT_DISPATCH;

      // 使用QueryInterface获取IDispatch接口
      HRESULT hr = drag_handler.Get()->QueryInterface(
          IID_IDispatch, reinterpret_cast<void**>(&drag_handler_variant.pdispVal));
      if (FAILED(hr)) {
        return std::unexpected("Failed to get IDispatch interface");
      }

      hr = webview2->AddHostObjectToScript(L"eventForwarder", &drag_handler_variant);
      if (FAILED(hr)) {
        return std::unexpected("Failed to add host object to script");
      }

      Logger().info("Drag handler registered successfully");

      // 注入预加载脚本
      std::wstring preload_script =
          LR"(
                window.addEventListener('DOMContentLoaded', () => {
                    document.body.addEventListener('mousedown', evt => {
                        const { target } = evt;
                        const appRegion = getComputedStyle(target)['app-region'];
                        
                        if (appRegion === 'drag') {
                            try {
                                chrome.webview.hostObjects.sync.eventForwarder.mouseDownDrag();
                                evt.preventDefault();
                                evt.stopPropagation();
                            } catch (e) {
                                console.error('Failed to trigger drag:', e);
                            }
                        }
                    });
                });
            )";

      // 使用正确的回调方式添加脚本
      webview2->AddScriptToExecuteOnDocumentCreated(
          preload_script.c_str(),
          Microsoft::WRL::Callback<
              ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>([](HRESULT error,
                                                                                   PCWSTR id)
                                                                                    -> HRESULT {
            if (FAILED(error)) {
              Logger().error("Failed to inject preload script for drag handling");
            } else {
              Logger().info("Preload script injected for drag handling");
            }
            return S_OK;
          }).Get());
    } else {
      return std::unexpected("WebView does not support required features");
    }

    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception in drag handler registration: ") + e.what());
  }
}
}  // namespace Core::WebView::DragHandler