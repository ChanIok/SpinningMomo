module;

module Features.Registry;

import std;
import Utils.Logger;

namespace Features::Registry {

auto create_registry() -> FeatureRegistry { return FeatureRegistry{}; }

auto register_feature(FeatureRegistry& registry, FeatureDescriptor descriptor) -> void {
  const auto& id = descriptor.id;

  if (registry.descriptors.contains(id)) {
    Logger().warn("Feature already registered: {}", id);
    return;
  }

  registry.descriptors.emplace(id, std::move(descriptor));
  registry.registration_order.push_back(id);

  Logger().debug("Registered feature: {}", id);
}

auto invoke_feature(FeatureRegistry& registry, const std::string& id) -> bool {
  auto it = registry.descriptors.find(id);
  if (it == registry.descriptors.end()) {
    Logger().warn("Feature not found: {}", id);
    return false;
  }

  if (!it->second.action) {
    Logger().warn("Feature has no action: {}", id);
    return false;
  }

  try {
    it->second.action();
    Logger().debug("Invoked feature: {}", id);
    return true;
  } catch (const std::exception& e) {
    Logger().error("Failed to invoke feature {}: {}", id, e.what());
    return false;
  }
}

auto get_all_features(const FeatureRegistry& registry) -> std::vector<FeatureDescriptor> {
  std::vector<FeatureDescriptor> result;
  result.reserve(registry.registration_order.size());

  for (const auto& id : registry.registration_order) {
    auto it = registry.descriptors.find(id);
    if (it != registry.descriptors.end()) {
      result.push_back(it->second);
    }
  }

  return result;
}

auto get_feature(const FeatureRegistry& registry, const std::string& id)
    -> std::optional<FeatureDescriptor> {
  auto it = registry.descriptors.find(id);
  if (it != registry.descriptors.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace Features::Registry
