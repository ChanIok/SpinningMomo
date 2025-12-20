module;

module Core.Commands;

import std;
import Utils.Logger;

namespace Core::Commands {

auto register_command(CommandRegistry& registry, CommandDescriptor descriptor) -> void {
  const std::string id = descriptor.id;

  if (registry.descriptors.contains(id)) {
    Logger().warn("Command already registered: {}", id);
    return;
  }

  registry.descriptors.emplace(id, std::move(descriptor));
  registry.registration_order.push_back(id);

  Logger().debug("Registered command: {}", id);
}

auto invoke_command(CommandRegistry& registry, const std::string& id) -> bool {
  auto it = registry.descriptors.find(id);
  if (it == registry.descriptors.end()) {
    Logger().warn("Command not found: {}", id);
    return false;
  }

  if (!it->second.action) {
    Logger().warn("Command has no action: {}", id);
    return false;
  }

  try {
    it->second.action();
    Logger().debug("Invoked command: {}", id);
    return true;
  } catch (const std::exception& e) {
    Logger().error("Failed to invoke command {}: {}", id, e.what());
    return false;
  }
}

auto get_all_commands(const CommandRegistry& registry) -> std::vector<CommandDescriptor> {
  std::vector<CommandDescriptor> result;
  result.reserve(registry.registration_order.size());

  for (const auto& id : registry.registration_order) {
    auto it = registry.descriptors.find(id);
    if (it != registry.descriptors.end()) {
      result.push_back(it->second);
    }
  }

  return result;
}

auto get_command(const CommandRegistry& registry, const std::string& id)
    -> std::optional<CommandDescriptor> {
  auto it = registry.descriptors.find(id);
  if (it != registry.descriptors.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace Core::Commands
