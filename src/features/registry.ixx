module;

export module Features.Registry;

import std;

namespace Features::Registry {

// 功能描述符
export struct FeatureDescriptor {
  std::string id;        // 唯一标识，如 "screenshot.capture"
  std::string i18n_key;  // i18n 键，如 "menu.screenshot_capture"

  bool is_toggle = false;  // 是否为切换类型

  std::function<void()> action;               // 点击执行的动作
  std::function<bool()> get_state = nullptr;  // toggle 类型：获取当前状态
};

// 运行时功能注册表
export struct FeatureRegistry {
  std::unordered_map<std::string, FeatureDescriptor> descriptors;
  std::vector<std::string> registration_order;  // 保持注册顺序
};

// === API ===

// 创建空注册表
export auto create_registry() -> FeatureRegistry;

// 注册功能
export auto register_feature(FeatureRegistry& registry, FeatureDescriptor descriptor) -> void;

// 调用功能
export auto invoke_feature(FeatureRegistry& registry, const std::string& id) -> bool;

// 获取所有功能描述符（按注册顺序）
export auto get_all_features(const FeatureRegistry& registry) -> std::vector<FeatureDescriptor>;

// 获取单个功能描述符
export auto get_feature(const FeatureRegistry& registry, const std::string& id)
    -> std::optional<FeatureDescriptor>;

// === RPC Types ===

// 用于 RPC 传输的功能描述符（不包含 function 字段）
export struct FeatureDescriptorData {
  std::string id;
  std::string i18n_key;
  bool is_toggle;
};

export struct GetAllFeaturesParams {
  // 空结构体，未来可扩展
};

export struct GetAllFeaturesResult {
  std::vector<FeatureDescriptorData> features;
};

}  // namespace Features::Registry

// Forward declaration
namespace Core::State {
export struct AppState;
}

namespace Features::Registry {

// 注册所有内置功能（需要在应用初始化时调用）
export auto register_builtin_features(Core::State::AppState& state, FeatureRegistry& registry)
    -> void;

}  // namespace Features::Registry
