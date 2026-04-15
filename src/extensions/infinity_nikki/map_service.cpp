module;

module Extensions.InfinityNikki.MapService;

import std;
import Core.State;
import Core.WebView;
import Extensions.InfinityNikki.Generated.MapInjectionScript;
import Utils.Logger;

namespace Extensions::InfinityNikki::MapService {

auto register_from_settings(Core::State::AppState& app_state) -> void {
#ifdef NDEBUG
  constexpr bool allow_dev_eval = false;
#else
  constexpr bool allow_dev_eval = true;
#endif

  std::wstring script = std::wstring(Extensions::InfinityNikki::Generated::map_bridge_script);

  const std::wstring allow_dev_eval_literal = allow_dev_eval ? L"true" : L"false";
  std::wstring script_with_eval_flag = script;
  const std::wstring placeholder = L"__ALLOW_DEV_EVAL__";
  if (const std::size_t pos = script_with_eval_flag.find(placeholder); pos != std::wstring::npos) {
    script_with_eval_flag.replace(pos, placeholder.length(), allow_dev_eval_literal);
  }

  Core::WebView::register_document_created_script(
      app_state, "extensions.infinity_nikki.map_service.bridge", script_with_eval_flag);
  Logger().info("InfinityNikki map WebView bridge script registered");
}

}  // namespace Extensions::InfinityNikki::MapService
