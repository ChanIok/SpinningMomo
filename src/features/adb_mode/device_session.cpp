#include "features/adb_mode/device_session.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/build_config.hpp"
#include "features/adb_mode/adb_client.hpp"
#include "features/adb_mode/display_control.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"

namespace features::adb_mode::session {

namespace {

constexpr auto kStartupTimeout = std::chrono::milliseconds(5000);
constexpr auto kStartupAttemptTimeout = std::chrono::milliseconds(1000);
constexpr auto kRequestTimeout = std::chrono::milliseconds(30000);
constexpr auto kStartupRetryDelay = std::chrono::milliseconds(100);
constexpr std::wstring_view kRemoteCaptureJar = L"/data/local/tmp/spinning-momo-capture.jar";

// 以大端序向缓冲区写入 16 位整数
auto write_u16(std::uint8_t* destination, std::uint16_t value) -> void {
  destination[0] = static_cast<std::uint8_t>(value >> 8u);
  destination[1] = static_cast<std::uint8_t>(value);
}

// 以大端序向缓冲区写入 32 位整数
auto write_u32(std::uint8_t* destination, std::uint32_t value) -> void {
  destination[0] = static_cast<std::uint8_t>(value >> 24u);
  destination[1] = static_cast<std::uint8_t>(value >> 16u);
  destination[2] = static_cast<std::uint8_t>(value >> 8u);
  destination[3] = static_cast<std::uint8_t>(value);
}

// 以大端序向缓冲区写入 64 位整数
auto write_u64(std::uint8_t* destination, std::uint64_t value) -> void {
  for (int index = 0; index < 8; ++index) {
    destination[index] = static_cast<std::uint8_t>(value >> (56 - index * 8));
  }
}

// 从缓冲区以大端序解析 16 位整数
auto read_u16(const std::uint8_t* source) -> std::uint16_t {
  return (static_cast<std::uint16_t>(source[0]) << 8u) | static_cast<std::uint16_t>(source[1]);
}

// 从缓冲区以大端序解析 32 位整数
auto read_u32(const std::uint8_t* source) -> std::uint32_t {
  return (static_cast<std::uint32_t>(source[0]) << 24u) |
         (static_cast<std::uint32_t>(source[1]) << 16u) |
         (static_cast<std::uint32_t>(source[2]) << 8u) | static_cast<std::uint32_t>(source[3]);
}

// 从缓冲区以大端序解析 64 位整数
auto read_u64(const std::uint8_t* source) -> std::uint64_t {
  std::uint64_t value = 0;
  for (int index = 0; index < 8; ++index) {
    value = (value << 8u) | source[index];
  }
  return value;
}

// 将 asio 错误码格式化为便于排查的可读文本
auto error_text(const asio::error_code& error, std::string_view operation) -> std::string {
  // 操作被主动取消通常意味着等待超时
  if (error == asio::error::operation_aborted) {
    return std::format("{} timed out", operation);
  }
  return std::format("{} failed: {}", operation, error.message());
}

// 带超时限制的同步 TCP 连接：挂定时器与连接任务，并等待首个完成
auto connect_with_timeout(asio::io_context& io_context, asio::ip::tcp::socket& socket,
                          const asio::ip::tcp::endpoint& endpoint,
                          std::chrono::milliseconds timeout) -> std::expected<void, std::string> {
  asio::error_code operation_error;
  bool completed = false;
  asio::steady_timer timer(io_context);

  // 超时后主动取消 socket 连接操作
  timer.expires_after(timeout);
  timer.async_wait([&](const asio::error_code& error) {
    if (!error && !completed) {
      completed = true;
      operation_error = asio::error::make_error_code(asio::error::operation_aborted);
      socket.cancel();
    }
  });

  // 发起异步连接并监听结果
  socket.async_connect(endpoint, [&](const asio::error_code& error) {
    if (completed) {
      return;
    }
    completed = true;
    operation_error = error;
    timer.cancel();
  });

  // 运行局部事件循环等待连接或超时
  io_context.restart();
  io_context.run();
  if (operation_error) {
    return std::unexpected(error_text(operation_error, "Android capture socket connection"));
  }
  return {};
}

// 带超时限制的定长读取：读满指定字节数，避免半包
auto read_exact(asio::io_context& io_context, asio::ip::tcp::socket& socket, void* data,
                std::size_t size, std::chrono::milliseconds timeout)
    -> std::expected<void, std::string> {
  asio::error_code operation_error;
  bool completed = false;
  asio::steady_timer timer(io_context);

  // 超时后主动取消读取操作
  timer.expires_after(timeout);
  timer.async_wait([&](const asio::error_code& error) {
    if (!error && !completed) {
      completed = true;
      operation_error = asio::error::make_error_code(asio::error::operation_aborted);
      socket.cancel();
    }
  });

  // 发起定长异步读取
  asio::async_read(socket, asio::buffer(data, size),
                   [&](const asio::error_code& error, std::size_t) {
                     if (completed) {
                       return;
                     }
                     completed = true;
                     operation_error = error;
                     timer.cancel();
                   });

  // 阻塞执行局部事件循环直到读取完成或超时
  io_context.restart();
  io_context.run();
  if (operation_error) {
    return std::unexpected(error_text(operation_error, "Android capture socket read"));
  }
  return {};
}

// 带超时限制的完整写出：确保所有负载数据均进入内核发送缓冲区
auto write_all(asio::io_context& io_context, asio::ip::tcp::socket& socket, const void* data,
               std::size_t size, std::chrono::milliseconds timeout)
    -> std::expected<void, std::string> {
  asio::error_code operation_error;
  bool completed = false;
  asio::steady_timer timer(io_context);

  // 超时后主动取消写入操作
  timer.expires_after(timeout);
  timer.async_wait([&](const asio::error_code& error) {
    if (!error && !completed) {
      completed = true;
      operation_error = asio::error::make_error_code(asio::error::operation_aborted);
      socket.cancel();
    }
  });

  // 发起完整异步写入
  asio::async_write(socket, asio::buffer(data, size),
                    [&](const asio::error_code& error, std::size_t) {
                      if (completed) {
                        return;
                      }
                      completed = true;
                      operation_error = error;
                      timer.cancel();
                    });

  // 阻塞等待写入完成或超时
  io_context.restart();
  io_context.run();
  if (operation_error) {
    return std::unexpected(error_text(operation_error, "Android capture socket write"));
  }
  return {};
}

// 封包并发送协议帧：组装 28 字节头部与负载 → 一次性写出
auto send_frame(DeviceSession& session, capture_protocol::MessageType type,
                std::uint32_t request_id, std::span<const std::uint8_t> payload,
                std::chrono::milliseconds timeout) -> std::expected<void, std::string> {
  // 校验 socket 就绪状态
  if (!session.io_context || !session.socket || !session.socket->is_open()) {
    return std::unexpected("Android capture socket is not open");
  }
  // 防止超大异常负载导致内存耗尽
  if (payload.size() > capture_protocol::kMaxPayloadSize) {
    return std::unexpected("Android capture payload is too large");
  }

  // 组装大端序二进制协议帧头
  std::vector<std::uint8_t> frame(capture_protocol::kHeaderSize + payload.size());
  write_u32(frame.data(), capture_protocol::kMagic);
  write_u16(frame.data() + 4, capture_protocol::kVersion);
  write_u16(frame.data() + 6, static_cast<std::uint16_t>(type));
  write_u32(frame.data() + 8, request_id);
  write_u32(frame.data() + 12, 0);
  write_u64(frame.data() + 16, 0);
  write_u32(frame.data() + 24, static_cast<std::uint32_t>(payload.size()));
  std::ranges::copy(payload, frame.begin() + capture_protocol::kHeaderSize);

  // 写出完整帧数据
  return write_all(*session.io_context, *session.socket, frame.data(), frame.size(), timeout);
}

// 接收并解析协议帧：读 28 字节头 → 校验魔数和版本 → 分配内存读取负载
auto receive_frame(DeviceSession& session, std::chrono::milliseconds timeout)
    -> std::expected<capture_protocol::Frame, std::string> {
  // 检查 socket 状态
  if (!session.io_context || !session.socket || !session.socket->is_open()) {
    return std::unexpected("Android capture socket is not open");
  }

  // 先读取定长 28 字节协议头部
  std::array<std::uint8_t, capture_protocol::kHeaderSize> header{};
  auto header_result =
      read_exact(*session.io_context, *session.socket, header.data(), header.size(), timeout);
  if (!header_result) {
    return std::unexpected(header_result.error());
  }

  // 校验魔数是否匹配 MOMO
  if (read_u32(header.data()) != capture_protocol::kMagic) {
    return std::unexpected("Android capture protocol magic is invalid");
  }
  // 校验协议版本号
  if (read_u16(header.data() + 4) != capture_protocol::kVersion) {
    return std::unexpected("Android capture protocol version is unsupported");
  }

  // 检查负载长度是否超过上限
  const auto payload_size = read_u32(header.data() + 24);
  if (payload_size > capture_protocol::kMaxPayloadSize) {
    return std::unexpected("Android capture payload exceeds the protocol limit");
  }

  // 构造协议帧对象
  capture_protocol::Frame frame{
      .type = static_cast<capture_protocol::MessageType>(read_u16(header.data() + 6)),
      .request_id = read_u32(header.data() + 8),
      .flags = read_u32(header.data() + 12),
      .timestamp = read_u64(header.data() + 16),
      .payload = std::vector<std::uint8_t>(payload_size),
  };

  // 存在负载时按长度完整读取
  if (payload_size > 0) {
    auto payload_result = read_exact(*session.io_context, *session.socket, frame.payload.data(),
                                     frame.payload.size(), timeout);
    if (!payload_result) {
      return std::unexpected(payload_result.error());
    }
  }
  return frame;
}

// 提取服务端在 ERROR 帧中返回的错误描述字符串
auto capture_error(const capture_protocol::Frame& frame) -> std::string {
  return std::string(frame.payload.begin(), frame.payload.end());
}

// 校验字节流是否包含合法的 PNG 文件头签名
auto is_valid_png(std::span<const std::uint8_t> image) -> bool {
  constexpr std::array<std::uint8_t, 8> signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  return image.size() >= signature.size() &&
         std::equal(signature.begin(), signature.end(), image.begin());
}

// 校验字节流是否包含合法的 JPEG SOI (0xFFD8) 与 EOI (0xFFD9) 标记
auto is_valid_jpeg(std::span<const std::uint8_t> image) -> bool {
  return image.size() >= 4 && image[0] == 0xFF && image[1] == 0xD8 &&
         image[image.size() - 2] == 0xFF && image[image.size() - 1] == 0xD9;
}

// 将图片数据安全写入磁盘：写入临时文件 → 校验完整性 → 原子重命名至目标路径
auto write_image_to_file(const std::filesystem::path& output_path,
                         std::span<const std::uint8_t> image) -> std::expected<void, std::string> {
  // 确保父目录存在
  const auto parent = output_path.parent_path();
  if (!parent.empty()) {
    auto ensure_result = utils::path::EnsureDirectoryExists(parent);
    if (!ensure_result) {
      return std::unexpected("Failed to create screenshot directory: " + ensure_result.error());
    }
  }

  // 构造并清理同名临时文件
  auto temporary_path = output_path;
  temporary_path += L".tmp";
  std::error_code remove_error;
  std::filesystem::remove(temporary_path, remove_error);

  // 完整写入临时文件
  {
    std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
    if (!file) {
      return std::unexpected("Failed to open temporary screenshot file: " +
                             temporary_path.string());
    }
    file.write(reinterpret_cast<const char*>(image.data()),
               static_cast<std::streamsize>(image.size()));
    if (!file) {
      file.close();
      std::filesystem::remove(temporary_path, remove_error);
      return std::unexpected("Failed to write screenshot file: " + temporary_path.string());
    }
  }

  // 原子重命名为目标路径，避免外部观察到半写状态
  std::error_code rename_error;
  std::filesystem::rename(temporary_path, output_path, rename_error);
  if (rename_error) {
    std::filesystem::remove(temporary_path, remove_error);
    return std::unexpected("Failed to finalize screenshot file: " + rename_error.message());
  }
  return {};
}

// 生成当前进程唯一的 Android 抽象 socket 名称
auto make_socket_name() -> std::string {
  static std::atomic<std::uint32_t> sequence = 0;
  return std::format("spinning-momo-capture-{}-{}", GetCurrentProcessId(),
                     sequence.fetch_add(1, std::memory_order_relaxed));
}

// 为长期运行的 adb shell 后台进程生成独立的 stderr 诊断日志文件路径
auto make_stderr_path() -> std::expected<std::filesystem::path, std::string> {
  std::error_code error;
  const auto directory = std::filesystem::temp_directory_path(error);
  if (error) {
    return std::unexpected("Failed to resolve process temporary directory: " + error.message());
  }

  static std::atomic<std::uint32_t> sequence = 0;
  return directory / std::format("spinning-momo-capture-{}-{}.log", GetCurrentProcessId(),
                                 sequence.fetch_add(1, std::memory_order_relaxed));
}

// 定位 Android 捕获服务的 JAR 包：Debug 从 build 目录查找，Release 从 resources 查找
auto resolve_capture_service_jar() -> std::expected<std::filesystem::path, std::string> {
  auto executable_directory = utils::path::GetExecutableDirectory();
  if (!executable_directory) {
    return std::unexpected("Failed to resolve executable directory for Android capture service: " +
                           executable_directory.error());
  }

  std::filesystem::path jar_path;
  if constexpr (core::build_config::is_debug_build()) {
    jar_path = executable_directory.value() / L".." / L".." / L".." / L".." / L"build" /
               L"android" / L"momo-capture.jar";
  } else {
    jar_path = executable_directory.value() / L"resources" / L"android" / L"momo-capture.jar";
  }
  jar_path = jar_path.lexically_normal();

  // 检查 JAR 文件是否存在
  std::error_code file_error;
  if (!std::filesystem::is_regular_file(jar_path, file_error)) {
    return std::unexpected("Android capture service JAR was not found: " + jar_path.string());
  }
  return jar_path;
}

// 附加子进程退出码与 stderr 诊断日志，丰富错误报告
auto append_process_diagnostics(std::string error, const DeviceSession& session) -> std::string {
  // 如果子进程已退出，记录其退出状态码
  auto exit_code = utils::process::get_exit_code(session.server_process);
  if (exit_code && exit_code.value() != STILL_ACTIVE) {
    error += std::format(" (adb shell exited with code {})", exit_code.value());
  }

  // 读取并附加服务进程输出的 stderr 文本
  auto stderr_result = utils::process::read_stderr(session.server_process);
  if (stderr_result) {
    const auto text = utils::string::TrimAscii(stderr_result.value());
    if (!text.empty()) {
      error += "; Android capture stderr: " + text;
    }
  }
  return error;
}

// 移除为捕获服务创建的 adb forward 端口映射
auto remove_forward(DeviceSession& session) -> std::expected<void, std::string> {
  if (session.local_port == 0) {
    return {};
  }

  // 调用 adb forward --remove 清除端口映射
  auto result = adb::run_on_device(
      session.config, session.config.serial,
      {L"forward", L"--remove", utils::string::FromUtf8(std::format("tcp:{}", session.local_port))},
      std::chrono::seconds(5));
  session.local_port = 0;
  if (!result) {
    return std::unexpected("Failed to remove Android capture socket forward: " + result.error());
  }
  return {};
}

// 安全断开并关闭会话的 TCP socket
auto close_socket(DeviceSession& session) -> void {
  if (!session.socket) {
    return;
  }
  if (session.socket->is_open()) {
    asio::error_code error;
    session.socket->shutdown(asio::ip::tcp::socket::shutdown_both, error);
    session.socket->close(error);
  }
  session.socket.reset();
}

// 在截止时间内连接或重建传输链路：校验进程存活 → 循环重试连接本地端口 → 读取 READY 握手帧
auto connect_transport(DeviceSession& session, std::chrono::milliseconds timeout)
    -> std::expected<void, std::string> {
  // socket 仍处于打开状态则直接复用
  if (session.socket && session.socket->is_open()) {
    return {};
  }
  close_socket(session);

  if (session.local_port == 0) {
    return std::unexpected("Android capture local port is not configured");
  }
  if (!session.io_context) {
    session.io_context = std::make_unique<asio::io_context>();
  }

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::string last_error = "Android capture service did not become ready";
  const asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::loopback(), session.local_port);

  while (true) {
    // 检查服务进程是否已异常退出，避免盲目空等
    auto exit_code = utils::process::get_exit_code(session.server_process);
    if (exit_code && exit_code.value() != STILL_ACTIVE) {
      return std::unexpected(
          append_process_diagnostics("Android capture process exited prematurely", session));
    }

    // 计算当前剩余可用总时长
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      break;
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

    // 单次连接尝试超时受剩余总时间与单次上限共同约束
    const auto attempt_timeout = std::min(remaining, kStartupAttemptTimeout);

    // 尝试连接本地转发端口
    session.socket = std::make_unique<asio::ip::tcp::socket>(*session.io_context);
    auto connect_result =
        connect_with_timeout(*session.io_context, *session.socket, endpoint, attempt_timeout);
    if (connect_result) {
      // 连接成功后等待首个 READY 握手帧，给足当前剩余总时间
      const auto handshake_now = std::chrono::steady_clock::now();
      if (handshake_now >= deadline) {
        last_error = "Android capture handshake timed out";
        close_socket(session);
        break;
      }
      const auto handshake_timeout =
          std::chrono::duration_cast<std::chrono::milliseconds>(deadline - handshake_now);

      auto ready_result = receive_frame(session, handshake_timeout);
      if (ready_result && ready_result->type == capture_protocol::MessageType::Ready) {
        return {};
      }
      last_error = ready_result ? "Android capture service returned an unexpected startup message"
                                : ready_result.error();
    } else {
      last_error = connect_result.error();
    }

    // 本次尝试失败，清理临时 socket 并延时重试
    close_socket(session);
    if (std::chrono::steady_clock::now() + kStartupRetryDelay >= deadline) {
      break;
    }
    std::this_thread::sleep_for(kStartupRetryDelay);
  }

  return std::unexpected(append_process_diagnostics(last_error, session));
}

// 停用捕获服务传输层：发送 SHUTDOWN 指令 → 关闭连接 → 终止后台进程 → 移除端口映射
auto stop_capture_transport(DeviceSession& session) -> std::expected<void, std::string> {
  std::optional<std::string> first_error;

  // 在协议锁保护下向服务端发送 SHUTDOWN 帧并等待应答
  {
    std::scoped_lock lock(session.protocol_mutex);
    if (session.socket && session.socket->is_open()) {
      auto send_result = send_frame(session, capture_protocol::MessageType::Shutdown, 0,
                                    std::span<const std::uint8_t>{}, kRequestTimeout);
      if (send_result) {
        auto ack_result = receive_frame(session, kRequestTimeout);
        if (!ack_result) {
          first_error = ack_result.error();
        } else if (ack_result->type != capture_protocol::MessageType::ShutdownAck) {
          first_error = "Android capture service did not acknowledge SHUTDOWN";
        }
      } else {
        first_error = send_result.error();
      }
    }
    close_socket(session);
  }

  // 终止 Windows 侧维持的 adb shell 后台进程
  auto terminate_result = utils::process::terminate(session.server_process);
  if (!terminate_result && !first_error) {
    first_error = terminate_result.error();
  }

  // 移除本地端口映射
  auto remove_result = remove_forward(session);
  if (!remove_result && !first_error) {
    first_error = remove_result.error();
  }

  // 清理临时诊断 stderr 文件
  std::error_code remove_error;
  if (!session.server_stderr_file.empty()) {
    std::filesystem::remove(session.server_stderr_file, remove_error);
    session.server_stderr_file.clear();
  }

  if (first_error) {
    return std::unexpected(*first_error);
  }
  return {};
}

// 会话打开中途失败时的清理回滚
auto abort_open(DeviceSession& session) -> void {
  // 停用已创建的传输层与服务端
  static_cast<void>(stop_capture_transport(session));
  // 若 ADB 连接是由本次调用创建，则负责断开该连接
  if (session.connection_owned) {
    static_cast<void>(adb::disconnect(session.config, session.config.serial));
  }
}

// 建立本地转发传输通道：建立单次 adb forward → 连接本地端口并完成握手
auto establish_transport(DeviceSession& session) -> std::expected<void, std::string> {
  // 建立 adb forward 映射到设备端的 abstract unix socket
  const auto remote_endpoint = utils::string::FromUtf8("localabstract:" + session.socket_name);
  auto forward_result =
      adb::run_on_device(session.config, session.config.serial,
                         {L"forward", L"tcp:0", remote_endpoint}, std::chrono::seconds(5));
  if (!forward_result) {
    return std::unexpected(append_process_diagnostics(
        "Failed to forward Android capture socket: " + forward_result.error(), session));
  }

  // 解析 adb forward 分配的本地随机端口号
  const auto text = utils::string::TrimAscii(forward_result->stdout_data);
  std::uint32_t port = 0;
  const auto parse_result = std::from_chars(text.data(), text.data() + text.size(), port);
  if (parse_result.ec != std::errc{} || parse_result.ptr != text.data() + text.size() ||
      port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
    static_cast<void>(remove_forward(session));
    return std::unexpected(
        append_process_diagnostics("ADB forward returned an invalid local port: " + text, session));
  }
  session.local_port = static_cast<std::uint16_t>(port);

  // 在超时期限内重试连接并等待服务端发出的就绪帧
  auto connect_result = connect_transport(session, kStartupTimeout);
  if (!connect_result) {
    static_cast<void>(remove_forward(session));
    return std::unexpected(connect_result.error());
  }
  return {};
}

// 启动设备端 Java 捕获服务进程：配置 stderr 日志 → 组装 app_process 命令行 → 启动长期进程
auto start_server(DeviceSession& session) -> std::expected<void, std::string> {
  // 分配 stderr 诊断日志文件路径
  auto stderr_path_result = make_stderr_path();
  if (!stderr_path_result) {
    return std::unexpected(stderr_path_result.error());
  }
  session.server_stderr_file = stderr_path_result.value();

  // 拼装 app_process 启动命令
  const auto server_command =
      std::format(L"CLASSPATH={} app_process / com.spinningmomo.capture.Main server --socket {}",
                  std::wstring(kRemoteCaptureJar), utils::string::FromUtf8(session.socket_name));

  // 通过 adb shell 启动后台服务进程
  auto process_result = utils::process::spawn(
      session.config.executable,
      {L"-s", utils::string::FromUtf8(session.config.serial), L"shell", server_command},
      utils::process::SpawnOptions{.stderr_file = session.server_stderr_file});
  if (!process_result) {
    std::error_code remove_error;
    std::filesystem::remove(session.server_stderr_file, remove_error);
    session.server_stderr_file.clear();
    return std::unexpected("Failed to start Android capture service: " + process_result.error());
  }
  session.server_process = std::move(process_result.value());
  return {};
}

}  // namespace

// 会话析构：释放 socket → 终止服务进程 → 移除日志文件
DeviceSession::~DeviceSession() {
  close_socket(*this);
  static_cast<void>(utils::process::terminate(server_process));
  std::error_code remove_error;
  if (!server_stderr_file.empty()) {
    std::filesystem::remove(server_stderr_file, remove_error);
  }
}

// 建立完整设备会话：连接设备 → 查询初始分辨率 → 推送 JAR 服务 → 启动服务端 → 建立 socket 传输
auto open(const AdbConnectionConfig& config)
    -> std::expected<std::shared_ptr<DeviceSession>, std::string> {
  // 建立 ADB 连接或复用已连接设备
  auto connection_result = adb::connect(config);
  if (!connection_result) {
    return std::unexpected(connection_result.error());
  }

  // 初始化会话对象基础信息
  auto session = std::make_shared<DeviceSession>();
  session->config = config;
  session->config.serial = connection_result->serial;
  session->connection_owned = connection_result->connected_by_us;
  session->socket_name = make_socket_name();

  // 读取设备物理分辨率作为基准，用于后续比例计算与还原
  auto display_result = display_control::query(session->config, session->config.serial);
  if (!display_result) {
    abort_open(*session);
    return std::unexpected(display_result.error());
  }
  session->physical_display = *display_result;
  session->current_display = *display_result;

  // 定位并推送服务端 JAR 文件到设备临时目录
  auto jar_result = resolve_capture_service_jar();
  if (!jar_result) {
    abort_open(*session);
    return std::unexpected(jar_result.error());
  }
  auto deploy_result = adb::run_on_device(
      session->config, session->config.serial,
      {L"push", jar_result->wstring(), std::wstring(kRemoteCaptureJar)}, std::chrono::seconds(60));
  if (!deploy_result) {
    abort_open(*session);
    return std::unexpected("Failed to deploy Android capture service: " + deploy_result.error());
  }

  // 启动设备端的 app_process 服务
  auto server_result = start_server(*session);
  if (!server_result) {
    abort_open(*session);
    return std::unexpected(server_result.error());
  }

  // 建立本地端口转发并完成握手连接
  auto transport_result = establish_transport(*session);
  if (!transport_result) {
    const auto error = transport_result.error();
    abort_open(*session);
    return std::unexpected(error);
  }

  return session;
}

// 关闭设备会话：停传输与服务 → 恢复初始分辨率 → 断开自建连接 → 释放会话指针
auto close(std::shared_ptr<DeviceSession>& session) -> std::expected<void, std::string> {
  if (!session) {
    return {};
  }

  std::optional<std::string> first_error;

  // 停用网络传输层与后台进程
  auto transport_result = stop_capture_transport(*session);
  if (!transport_result) {
    first_error = transport_result.error();
  }

  // 退出时恢复设备的物理显示分辨率
  auto restore_result = display_control::reset(session->config, session->config.serial);
  if (!restore_result && !first_error) {
    first_error = restore_result.error();
  }

  // 如果连接由本程序主动建立，则负责主动断开
  if (session->connection_owned) {
    auto disconnect_result = adb::disconnect(session->config, session->config.serial);
    if (!disconnect_result && !first_error) {
      first_error = disconnect_result.error();
    }
  }

  // 释放会话所有权
  session.reset();
  if (first_error) {
    return std::unexpected(*first_error);
  }
  return {};
}

// 恢复物理显示尺寸：重置 wm size → 更新当前会话尺寸快照
auto restore_display(DeviceSession& session) -> std::expected<void, std::string> {
  auto result = display_control::reset(session.config, session.config.serial);
  if (result) {
    std::scoped_lock lock(session.metadata_mutex);
    session.current_display = session.physical_display;
  }
  return result;
}

// 设置设备显示尺寸：校验参数 → 执行 wm size override → 更新当前会话尺寸快照
auto set_display(DeviceSession& session, const Resolution& target)
    -> std::expected<void, std::string> {
  if (target.width <= 0 || target.height <= 0) {
    return std::unexpected("ADB device display size must be positive");
  }

  auto result = display_control::set(session.config, session.config.serial, target);
  if (result) {
    std::scoped_lock lock(session.metadata_mutex);
    session.current_display = target;
  }
  return result;
}

// 请求单张屏幕截图并保存至文件：确保连接就绪 → 发请求帧 → 读响应帧 → 校验魔数 → 原子写盘
auto screenshot_to_file(DeviceSession& session, const std::filesystem::path& output_path,
                        AdbScreenshotFormat format) -> std::expected<void, std::string> {
  if (output_path.empty()) {
    return std::unexpected("Screenshot output path cannot be empty");
  }

  // 在协议互斥锁保护下保证请求与响应的原子对应
  std::scoped_lock lock(session.protocol_mutex);

  // 确保传输连接就绪；若先前发生超时或断开则自动重连
  auto connect_result = connect_transport(session, kStartupTimeout);
  if (!connect_result) {
    return std::unexpected(connect_result.error());
  }

  const auto request_id = session.next_request_id++;
  const std::array<std::uint8_t, 2> request = {
      static_cast<std::uint8_t>(format == AdbScreenshotFormat::PNG ? 0 : 1), 100};

  // 发送截图请求帧
  auto send_result = send_frame(session, capture_protocol::MessageType::ScreenshotRequest,
                                request_id, request, kRequestTimeout);
  if (!send_result) {
    close_socket(session);
    return std::unexpected(send_result.error());
  }

  // 接收服务端返回的图片响应帧
  auto response_result = receive_frame(session, kRequestTimeout);
  if (!response_result) {
    close_socket(session);
    return std::unexpected(response_result.error());
  }

  // 校验响应帧 request_id 是否与本次请求一致
  if (response_result->request_id != request_id) {
    close_socket(session);
    return std::unexpected("Android capture response request id does not match");
  }

  // 如果服务端返回错误帧，提取错误信息（协议未失步，保留连接）
  if (response_result->type == capture_protocol::MessageType::Error) {
    return std::unexpected(capture_error(response_result.value()));
  }

  // 校验消息类型是否为图片响应
  if (response_result->type != capture_protocol::MessageType::ImageResponse) {
    close_socket(session);
    return std::unexpected("Android capture service returned an unexpected screenshot response");
  }

  // 校验二进制图片格式有效性
  const auto image = std::span<const std::uint8_t>(response_result->payload);
  const bool valid =
      format == AdbScreenshotFormat::PNG ? is_valid_png(image) : is_valid_jpeg(image);
  if (!valid) {
    return std::unexpected("Android capture service returned an invalid screenshot");
  }

  // 将图片安全写入目标文件路径
  return write_image_to_file(output_path, image);
}

}  // namespace features::adb_mode::session
