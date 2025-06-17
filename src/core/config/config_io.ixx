module;

export module Core.Config.Io;

import std;
import Core.Config;
import Common.Types;
import Core.Constants;

export namespace Core::Config::Io {

// ============================================================================
// 主要的配置 I/O 函数
// ============================================================================

/**
 * @brief 初始化配置。
 *
 * 寻找配置文件路径，如果文件不存在则创建默认配置，然后从文件加载所有配置项。
 * 成功时返回填充好的 AppConfig 结构体。
 * 失败时返回包含错误信息的字符串。
 *
 * @return std::expected<AppConfig, std::string>
 */
auto initialize() -> std::expected<AppConfig, std::string>;

/**
 * @brief 将配置保存到文件。
 *
 * 将传入的 AppConfig 结构体的所有内容写回到 .ini 配置文件中。
 *
 * @param config 要保存的配置对象。
 * @return std::expected<void, std::string> 成功则为空，失败则为错误信息。
 */
auto save(const AppConfig& config) -> std::expected<void, std::string>;

// ============================================================================
// 从配置中解析特定数据列表的函数
// ============================================================================

/**
 * @brief 根据配置和本地化字符串获取宽高比列表。
 * @param config 应用配置。
 * @param strings 本地化字符串资源。
 * @return RatioLoadResult 包含宽高比列表或错误信息。
 */
auto get_aspect_ratios(const AppConfig& config, const Constants::LocalizedStrings& strings)
    -> RatioLoadResult;

/**
 * @brief 根据配置和本地化字符串获取分辨率预设列表。
 * @param config 应用配置。
 * @param strings 本地化字符串资源。
 * @return ResolutionLoadResult 包含分辨率列表或错误信息。
 */
auto get_resolution_presets(const AppConfig& config, const Constants::LocalizedStrings& strings)
    -> ResolutionLoadResult;

}  // namespace Core::Config::Io