module;

export module Core.I18n.Types;

import std;

export namespace Core::I18n::Types {

enum class Language { ZhCN, EnUS };

// 扁平化文本数据 - 使用 key-value 映射
// key 格式: "category.item" (例如: "menu.app_main", "message.window_not_found")
using TextData = std::unordered_map<std::string, std::string>;

// 辅助函数：安全获取文本，如果不存在返回key本身
inline auto get_text(const TextData& texts, const std::string& key) -> std::string {
  auto it = texts.find(key);
  return (it != texts.end()) ? it->second : key;
}

// 辅助函数：创建默认文本数据（空map）
inline auto create_default_text_data() -> TextData { return TextData{}; }

}  // namespace Core::I18n::Types