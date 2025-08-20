module;

module Utils.File.Mime;

import std;

namespace Utils::File::Mime {

// MIME类型映射表
const std::unordered_map<std::string, std::string> mime_map = {
    // 文本类型
    {".html", "text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css; charset=utf-8"},
    {".js", "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".txt", "text/plain; charset=utf-8"},
    {".xml", "application/xml; charset=utf-8"},
    {".csv", "text/csv; charset=utf-8"},

    // 图像类型
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
    {".bmp", "image/bmp"},
    {".ico", "image/x-icon"},
    {".tiff", "image/tiff"},
    {".tif", "image/tiff"},

    // 视频类型
    {".mp4", "video/mp4"},
    {".webm", "video/webm"},
    {".avi", "video/x-msvideo"},
    {".mov", "video/quicktime"},
    {".wmv", "video/x-ms-wmv"},
    {".flv", "video/x-flv"},
    {".mkv", "video/x-matroska"},

    // 音频类型
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".ogg", "audio/ogg"},
    {".aac", "audio/aac"},
    {".flac", "audio/flac"},
    {".m4a", "audio/mp4"},

    // 文档类型
    {".pdf", "application/pdf"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},

    // 压缩文件
    {".zip", "application/zip"},
    {".rar", "application/vnd.rar"},
    {".7z", "application/x-7z-compressed"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},

    // 字体文件
    {".ttf", "font/ttf"},
    {".otf", "font/otf"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".eot", "application/vnd.ms-fontobject"},

    // 其他常见类型
    {".exe", "application/octet-stream"},
    {".dll", "application/octet-stream"},
    {".bin", "application/octet-stream"},
    {".iso", "application/octet-stream"}};

auto get_mime_type_by_extension(const std::string &extension) -> std::string {
  // 转换为小写进行匹配
  std::string lowercase_ext = extension;
  std::transform(lowercase_ext.begin(), lowercase_ext.end(), lowercase_ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  auto it = mime_map.find(lowercase_ext);
  if (it != mime_map.end()) {
    return it->second;
  }

  // 默认返回二进制流类型
  return "application/octet-stream";
}

auto get_mime_type(const std::filesystem::path &file_path) -> std::string {
  std::string extension = file_path.extension().string();
  return get_mime_type_by_extension(extension);
}

auto get_mime_type(const std::string &file_path) -> std::string {
  std::filesystem::path fs_path(file_path);
  return get_mime_type(fs_path);
}

}  // namespace Utils::File::Mime