#include "core/rpc/registry.hpp"

#include "vendor/std.hpp"

#include "core/rpc/endpoints/backup/backup.hpp"
#include "core/rpc/endpoints/clipboard/clipboard.hpp"
#include "core/rpc/endpoints/dialog/dialog.hpp"
#include "core/rpc/endpoints/extensions/extensions.hpp"
#include "core/rpc/endpoints/file/file.hpp"
#include "core/rpc/endpoints/gallery/gallery.hpp"
#include "core/rpc/endpoints/lan_access/lan_access.hpp"
#include "core/rpc/endpoints/registry/registry.hpp"
#include "core/rpc/endpoints/runtime_info/runtime_info.hpp"
#include "core/rpc/endpoints/settings/settings.hpp"
#include "core/rpc/endpoints/tasks/tasks.hpp"
#include "core/rpc/endpoints/update/update.hpp"
#include "core/rpc/endpoints/webview/webview.hpp"
#include "core/rpc/endpoints/window_control/window_control.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"

namespace core::rpc::registry {

// 注册所有RPC端点
auto register_all_endpoints(core::AppState& state) -> void {
  Logger().info("Starting RPC endpoints registration...");

  // 注册文件操作端点
  endpoints::file::register_all(state);

  // 注册数据备份与恢复端点
  endpoints::backup::register_all(state);

  // 注册剪贴板端点
  endpoints::clipboard::register_all(state);

  // 注册应用运行时信息端点
  endpoints::runtime_info::register_all(state);

  // 注册设置端点
  endpoints::settings::register_all(state);

  // 注册后台任务端点
  endpoints::tasks::register_all(state);

  // 注册功能注册表端点
  endpoints::registry::register_all(state);

  // 注册对话框端点
  endpoints::dialog::register_all(state);

  // 注册更新端点
  endpoints::update::register_all(state);

  // 注册Webview端点
  endpoints::webview::register_all(state);

  // 注册Gallery端点
  endpoints::gallery::register_all(state);

  // 注册本机 LAN 设置接口；接口自身仍要求 local 访问等级。
  endpoints::lan_access::register_all(state);

  // 注册拓展端点
  endpoints::extensions::register_all(state);

  // 注册窗口控制端点
  endpoints::window_control::register_all(state);

  Logger().info("RPC endpoints registration completed");
}

}  // namespace core::rpc::registry
