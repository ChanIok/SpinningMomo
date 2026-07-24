#include "core/rpc/registry.hpp"

#include "core/rpc/endpoints/backup/backup.hpp"
#include "core/rpc/endpoints/clipboard/clipboard.hpp"
#include "core/rpc/endpoints/dialog/dialog.hpp"
#include "core/rpc/endpoints/extensions/extensions.hpp"
#include "core/rpc/endpoints/file/file.hpp"
#include "core/rpc/endpoints/gallery/gallery.hpp"
#include "core/rpc/endpoints/registry/registry.hpp"
#include "core/rpc/endpoints/runtime_info/runtime_info.hpp"
#include "core/rpc/endpoints/settings/settings.hpp"
#include "core/rpc/endpoints/tasks/tasks.hpp"
#include "core/rpc/endpoints/update/update.hpp"
#include "core/rpc/endpoints/webview/webview.hpp"
#include "core/rpc/endpoints/window_control/window_control.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"

namespace Core::RPC::Registry {

// 注册所有RPC端点
auto register_all_endpoints(Core::State::AppState& state) -> void {
  Logger().info("Starting RPC endpoints registration...");

  // 注册文件操作端点
  Endpoints::File::register_all(state);

  // 注册数据备份与恢复端点
  Endpoints::Backup::register_all(state);

  // 注册剪贴板端点
  Endpoints::Clipboard::register_all(state);

  // 注册应用运行时信息端点
  Endpoints::RuntimeInfo::register_all(state);

  // 注册设置端点
  Endpoints::Settings::register_all(state);

  // 注册后台任务端点
  Endpoints::Tasks::register_all(state);

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

  // 注册拓展端点
  Endpoints::Extensions::register_all(state);

  // 注册窗口控制端点
  Endpoints::WindowControl::register_all(state);

  Logger().info("RPC endpoints registration completed");
}

}  // namespace Core::RPC::Registry
