module;

#include <wil/com.h>

#include <WebView2.h>  // 必须放最后面

module Core.WebView.Static;

import std;
import Core.State;
import Core.WebView.State;
import Core.WebView.Types;
import Utils.Logger;
import Utils.String;
import Vendor.BuildConfig;
import <Shlwapi.h>;
import <windows.h>;
import <wrl.h>;

namespace Core::WebView::Static {

auto try_web_resource_resolve(Core::State::AppState& state, std::wstring_view url)
    -> std::optional<Types::WebResourceResolution> {
  if (!state.webview || !state.webview->resources.web_resolvers) {
    return std::nullopt;
  }

  auto& registry = *state.webview->resources.web_resolvers;
  // 无锁读取（RCU 模式）
  auto resolvers = registry.resolvers.load();

  for (const auto& entry : *resolvers) {
    if (url.starts_with(entry.prefix)) {
      auto result = entry.resolver(url);
      if (result.success) {
        return result;
      }
    }
  }

  return std::nullopt;
}

auto handle_custom_web_resource_request(Core::State::AppState& state,
                                        ICoreWebView2Environment* environment,
                                        ICoreWebView2WebResourceRequestedEventArgs* args)
    -> HRESULT {
  if (!environment) {
    return S_OK;
  }

  wil::com_ptr<ICoreWebView2WebResourceRequest> request;
  if (FAILED(args->get_Request(request.put())) || !request) {
    return S_OK;
  }

  wil::unique_cotaskmem_string uri_raw;
  if (FAILED(request->get_Uri(&uri_raw)) || !uri_raw) {
    return S_OK;
  }

  std::wstring uri(uri_raw.get());

  auto custom_result = try_web_resource_resolve(state, uri);
  if (!custom_result) {
    return S_OK;
  }

  const auto& resolution = *custom_result;
  if (!resolution.success) {
    Logger().warn("WebView resource resolution failed: {}", resolution.error_message);

    wil::com_ptr<ICoreWebView2WebResourceResponse> not_found_response;
    if (SUCCEEDED(environment->CreateWebResourceResponse(nullptr, 404, L"Not Found", nullptr,
                                                         not_found_response.put()))) {
      args->put_Response(not_found_response.get());
    }
    return S_OK;
  }

  wil::com_ptr<IStream> stream;
  HRESULT hr =
      SHCreateStreamOnFileEx(resolution.file_path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
                             FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, stream.put());
  if (FAILED(hr) || !stream) {
    Logger().error("Failed to open custom resource file: {} (hr={})",
                   Utils::String::ToUtf8(resolution.file_path.wstring()), hr);
    return S_OK;
  }

  std::wstring headers;
  if (resolution.content_type) {
    headers = std::format(L"Content-Type: {}\r\nCache-Control: public, max-age=86400\r\n",
                          *resolution.content_type);
  } else {
    headers = L"Cache-Control: public, max-age=86400\r\n";
  }

  wil::com_ptr<ICoreWebView2WebResourceResponse> response;
  int status_code = resolution.status_code.value_or(200);
  const wchar_t* status_text = status_code == 200 ? L"OK" : L"Error";

  if (FAILED(environment->CreateWebResourceResponse(stream.get(), status_code, status_text,
                                                    headers.c_str(), response.put())) ||
      !response) {
    Logger().error("Failed to create WebView2 response for custom resource {}",
                   Utils::String::ToUtf8(resolution.file_path.wstring()));
    return S_OK;
  }

  args->put_Response(response.get());
  Logger().debug("Served custom WebView resource: {}",
                 Utils::String::ToUtf8(resolution.file_path.wstring()));
  return S_OK;
}

auto register_web_resource_resolver(Core::State::AppState& state, std::wstring prefix,
                                    Types::WebResourceResolver resolver) -> void {
  if (!state.webview || !state.webview->resources.web_resolvers) {
    Logger().error("WebView state not initialized, cannot register resource resolver");
    return;
  }

  auto& registry = *state.webview->resources.web_resolvers;
  std::unique_lock lock(registry.write_mutex);

  // RCU 写入：复制当前 vector，添加新项，然后原子替换
  auto current = registry.resolvers.load();
  auto new_resolvers = std::make_shared<std::vector<Types::WebResolverEntry>>(*current);
  new_resolvers->push_back({std::move(prefix), std::move(resolver)});
  registry.resolvers.store(new_resolvers);

  Logger().debug("Registered WebView resource resolver for: {}", Utils::String::ToUtf8(prefix));
}

auto unregister_web_resource_resolver(Core::State::AppState& state, std::wstring_view prefix)
    -> void {
  if (!state.webview || !state.webview->resources.web_resolvers) {
    return;
  }

  auto& registry = *state.webview->resources.web_resolvers;
  std::unique_lock lock(registry.write_mutex);

  // RCU 写入：复制当前 vector，删除匹配项，然后原子替换
  auto current = registry.resolvers.load();
  auto new_resolvers = std::make_shared<std::vector<Types::WebResolverEntry>>(*current);
  std::erase_if(*new_resolvers, [prefix](const auto& entry) { return entry.prefix == prefix; });
  registry.resolvers.store(new_resolvers);

  Logger().debug("Unregistered WebView resource resolver for: {}",
                 Utils::String::ToUtf8(std::wstring(prefix)));
}

auto setup_resource_interception(Core::State::AppState& state, ICoreWebView2* webview,
                                 ICoreWebView2Environment* environment,
                                 Core::WebView::State::CoreResources& resources,
                                 Core::WebView::State::WebViewConfig& config) -> HRESULT {
  auto webview2 = wil::com_ptr<ICoreWebView2>(webview).try_query<ICoreWebView2_2>();
  if (!webview2) {
    Logger().warn("ICoreWebView2_2 interface not available, custom resource interception disabled");
    return S_OK;
  }

  std::wstring filter;
  if (Vendor::BuildConfig::is_debug_build()) {
    filter = config.dev_server_url + L"/static/*";
    Logger().info("Debug mode: Intercepting static resources from {}",
                  Utils::String::ToUtf8(filter));
  } else {
    filter = L"https://" + config.static_host_name + L"/*";
    Logger().info("Release mode: Intercepting static resources from {}",
                  Utils::String::ToUtf8(filter));
  }

  HRESULT hr = webview2->AddWebResourceRequestedFilter(filter.c_str(),
                                                       COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE);
  if (FAILED(hr)) {
    Logger().warn("Failed to add WebResourceRequested filter: {}", hr);
    return hr;
  }

  auto app_state_ptr = &state;
  auto web_resource_requested_handler =
      Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
          [app_state_ptr, environment](
              ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
            return handle_custom_web_resource_request(*app_state_ptr, environment, args);
          });

  EventRegistrationToken token;
  hr = webview->add_WebResourceRequested(web_resource_requested_handler.Get(), &token);
  if (FAILED(hr)) {
    Logger().warn("Failed to register WebResourceRequested handler: {}", hr);
    return hr;
  }

  resources.webresource_requested_tokens.push_back(token);
  Logger().info("Custom WebResourceRequested handler registered");
  return S_OK;
}

}  // namespace Core::WebView::Static
