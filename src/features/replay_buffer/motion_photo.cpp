module;

module Features.ReplayBuffer.MotionPhoto;

import std;
import Utils.Logger;

namespace Features::ReplayBuffer::MotionPhoto {

// 构造 XMP 元数据 XML
auto build_xmp_xml(std::int64_t mp4_size, std::int64_t presentation_timestamp_us) -> std::string {
  return std::format(
      R"(<?xpacket begin='' id='W5M0MpCehiHzreSzNTczkc9d'?>)"
      R"(<x:xmpmeta xmlns:x="adobe:ns:meta/">)"
      R"(<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">)"
      R"(<rdf:Description )"
      R"(xmlns:GCamera="http://ns.google.com/photos/1.0/camera/" )"
      R"(xmlns:Container="http://ns.google.com/photos/1.0/container/" )"
      R"(xmlns:Item="http://ns.google.com/photos/1.0/container/item/" )"
      R"(GCamera:MotionPhoto="1" )"
      R"(GCamera:MotionPhotoVersion="1" )"
      R"(GCamera:MotionPhotoPresentationTimestampUs="{}">)"
      R"(<Container:Directory><rdf:Seq>)"
      R"(<rdf:li Item:Mime="image/jpeg" Item:Semantic="Primary"/>)"
      R"(<rdf:li Item:Mime="video/mp4" Item:Semantic="MotionPhoto" Item:Length="{}"/>)"
      R"(</rdf:Seq></Container:Directory>)"
      R"(</rdf:Description>)"
      R"(</rdf:RDF>)"
      R"(</x:xmpmeta>)"
      R"(<?xpacket end='w'?>)",
      presentation_timestamp_us, mp4_size);
}

// 将 XMP 注入 JPEG（作为 APP1 段）
// JPEG 结构: FF D8 [APP0...] [APP1...] ... 图像数据 ... FF D9
// 我们在 FF D8 后插入 APP1 段
auto inject_xmp_into_jpeg(const std::vector<uint8_t>& jpeg_data, const std::string& xmp_xml)
    -> std::expected<std::vector<uint8_t>, std::string> {
  if (jpeg_data.size() < 2 || jpeg_data[0] != 0xFF || jpeg_data[1] != 0xD8) {
    return std::unexpected("Invalid JPEG data: missing SOI marker");
  }

  // 构造 APP1 段
  // APP1 marker: FF E1
  // 长度: 2 字节（包含长度本身）+ namespace header + XMP 数据
  std::string xmp_namespace = "http://ns.adobe.com/xap/1.0/";
  xmp_namespace += '\0';  // null terminator

  size_t payload_size = xmp_namespace.size() + xmp_xml.size();
  size_t segment_length = 2 + payload_size;  // 2 bytes for length field itself

  if (segment_length > 0xFFFF) {
    return std::unexpected("XMP data too large for APP1 segment");
  }

  // 构建结果
  std::vector<uint8_t> result;
  result.reserve(jpeg_data.size() + 2 + segment_length);

  // 1. SOI marker (FF D8)
  result.push_back(0xFF);
  result.push_back(0xD8);

  // 2. APP1 segment
  result.push_back(0xFF);
  result.push_back(0xE1);
  result.push_back(static_cast<uint8_t>((segment_length >> 8) & 0xFF));
  result.push_back(static_cast<uint8_t>(segment_length & 0xFF));

  // XMP namespace header
  result.insert(result.end(), xmp_namespace.begin(), xmp_namespace.end());

  // XMP data
  result.insert(result.end(), xmp_xml.begin(), xmp_xml.end());

  // 3. 原始 JPEG 数据（跳过 SOI）
  result.insert(result.end(), jpeg_data.begin() + 2, jpeg_data.end());

  return result;
}

auto create_motion_photo(const std::filesystem::path& jpeg_path,
                         const std::filesystem::path& mp4_path,
                         const std::filesystem::path& output_path,
                         std::int64_t presentation_timestamp_us)
    -> std::expected<void, std::string> {
  try {
    // 1. 读取 JPEG 文件
    std::ifstream jpeg_file(jpeg_path, std::ios::binary);
    if (!jpeg_file) {
      return std::unexpected("Failed to open JPEG file: " + jpeg_path.string());
    }
    std::vector<uint8_t> jpeg_data((std::istreambuf_iterator<char>(jpeg_file)),
                                   std::istreambuf_iterator<char>());
    jpeg_file.close();

    // 2. 获取 MP4 文件大小
    std::error_code ec;
    auto mp4_size = std::filesystem::file_size(mp4_path, ec);
    if (ec) {
      return std::unexpected("Failed to get MP4 file size: " + ec.message());
    }

    // 3. 构造 XMP 元数据
    auto xmp_xml = build_xmp_xml(static_cast<std::int64_t>(mp4_size), presentation_timestamp_us);

    // 4. 将 XMP 注入 JPEG
    auto injected_result = inject_xmp_into_jpeg(jpeg_data, xmp_xml);
    if (!injected_result) {
      return std::unexpected(injected_result.error());
    }

    // 5. 写入输出文件: [注入XMP的JPEG] [MP4]
    std::ofstream output_file(output_path, std::ios::binary);
    if (!output_file) {
      return std::unexpected("Failed to create output file: " + output_path.string());
    }

    // 写入带 XMP 的 JPEG
    output_file.write(reinterpret_cast<const char*>(injected_result->data()),
                      injected_result->size());

    // 追加 MP4 数据
    std::ifstream mp4_file(mp4_path, std::ios::binary);
    if (!mp4_file) {
      return std::unexpected("Failed to open MP4 file: " + mp4_path.string());
    }

    // 分块复制以避免一次性加载大文件
    constexpr size_t chunk_size = 64 * 1024;  // 64KB
    std::vector<char> buffer(chunk_size);
    while (mp4_file.read(buffer.data(), chunk_size) || mp4_file.gcount() > 0) {
      output_file.write(buffer.data(), mp4_file.gcount());
    }

    output_file.close();

    Logger().info("Motion Photo created: {}", output_path.string());
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::format("Motion Photo creation failed: {}", e.what()));
  }
}

}  // namespace Features::ReplayBuffer::MotionPhoto
