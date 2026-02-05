module;

module Core.RPC.Registry;

import std;
import Core.State;
import Core.RPC.Endpoints.Dialog;
import Core.RPC.Endpoints.File;
import Core.RPC.Endpoints.Settings;
import Core.RPC.Endpoints.Registry;
import Core.RPC.Endpoints.Update;
import Core.RPC.Endpoints.WebView;
import Core.RPC.Endpoints.Gallery;
import Core.RPC.Endpoints.PluginEndpoints;
import Utils.Logger;

namespace Core::RPC::Registry {

// 注册所有RPC端点
auto register_all_endpoints(Core::State::AppState& state) -> void {
  Logger().info("Starting RPC endpoints registration...");

  // 注册文件操作端点
  Endpoints::File::register_all(state);

  // 注册设置端点
  Endpoints::Settings::register_all(state);

  // 注册功能注册表端点
  Endpoints::Registry::register_all(state);

  // 注册对话框端点
  Endpoints::Dialog::register_all(state);

  // 注册更新端点
  Endpoints::Update::register_all(state);

  // 注册Webview端点
  Endpoints::WebView::register_all(state);

  // 注册Gallery端点
  Endpoints::Gallery::register_all(state);

  // 注册插件端点
  Endpoints::PluginEndpoints::register_all(state);

  Logger().info("RPC endpoints registration completed");
}

}  // namespace Core::RPC::Registry