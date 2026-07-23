module;

export module Core.WebView.Types;

import std;

namespace Core::WebView::Types {

export struct WebResourceResolution {
  bool success;
  std::filesystem::path file_path;
  std::string error_message;
  std::optional<std::wstring> content_type;
  std::optional<int> status_code;
  std::optional<std::wstring> cache_control_header;
};

export using WebResourceResolver =
    std::move_only_function<WebResourceResolution(std::wstring_view) const>;

export struct WebResolverEntry {
  std::wstring prefix;
  WebResourceResolver resolver;
};

// WebView 资源解析器注册表
export struct WebResolverRegistry {
  std::vector<WebResolverEntry> resolvers;
  // 资源请求并发读取，注册操作独占写入。
  // resolver 执行期间不得修改同一个注册表，避免共享锁升级死锁。
  std::shared_mutex mutex;
};

}  // namespace Core::WebView::Types
