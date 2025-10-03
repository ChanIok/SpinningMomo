module;

export module Core.WebView.Types;

import std;

export namespace Core::WebView::Types {

struct WebResourceResolution {
  bool success;
  std::filesystem::path file_path;
  std::string error_message;
  std::optional<std::wstring> content_type;
  std::optional<int> status_code;
};

using WebResourceResolver = std::function<WebResourceResolution(std::wstring_view)>;

struct WebResolverEntry {
  std::wstring prefix;
  WebResourceResolver resolver;
};

struct WebResolverRegistry {
  std::vector<WebResolverEntry> resolvers;
  mutable std::shared_mutex mutex;
};

}  // namespace Core::WebView::Types
