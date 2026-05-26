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

export using WebResourceResolver = std::function<WebResourceResolution(std::wstring_view)>;

export struct WebResolverEntry {
  std::wstring prefix;
  WebResourceResolver resolver;
};

// 使用 RCU 模式的注册表（无锁读取）
export struct WebResolverRegistry {
  // 使用 atomic shared_ptr 实现无锁读取（RCU 模式）
  // 读取时无需加锁，写入时复制整个 vector
  std::atomic<std::shared_ptr<const std::vector<WebResolverEntry>>> resolvers{
      std::make_shared<const std::vector<WebResolverEntry>>()};

  // 写锁：仅用于保护写操作之间的竞争
  std::mutex write_mutex;
};

}  // namespace Core::WebView::Types
