#include "vendor/std.hpp"

#include "vendor/doctest.hpp"

#include "utils/path/path.hpp"

using utils::path::ClassifyPathStorageKind;
using utils::path::IsPathWithinBase;
using utils::path::NormalizeForComparison;
using utils::path::PathStorageKind;
using utils::path::TryParseUncServer;
using utils::path::TryResolveNetworkServer;

// 标准 UNC 与扩展 UNC 路径都应解析出同一个服务器段
TEST_CASE("UNC parser accepts supported Windows network path forms") {
  const auto standard_server = TryParseUncServer(LR"(\\server\share\photo.jpg)");
  const auto forward_slash_server = TryParseUncServer(L"//server/share/photo.jpg");
  const auto extended_server = TryParseUncServer(LR"(\\?\UNC\server\share\photo.jpg)");
  const auto device_server = TryParseUncServer(LR"(\\.\UNC\server\share\photo.jpg)");

  REQUIRE(standard_server.has_value());
  REQUIRE(forward_slash_server.has_value());
  REQUIRE(extended_server.has_value());
  REQUIRE(device_server.has_value());
  CHECK(*standard_server == L"server");
  CHECK(*forward_slash_server == L"server");
  CHECK(*extended_server == L"server");
  CHECK(*device_server == L"server");
}

// 本地路径和缺少共享名的网络地址不能被误判为可用 UNC 文件路径
TEST_CASE("UNC parser rejects local and incomplete paths") {
  CHECK_FALSE(TryParseUncServer(LR"(C:\Photos\photo.jpg)").has_value());
  CHECK_FALSE(TryParseUncServer(LR"(\\?\C:\Photos\photo.jpg)").has_value());
  CHECK_FALSE(TryParseUncServer(LR"(\\server)").has_value());
  CHECK_FALSE(TryParseUncServer(LR"(relative\photo.jpg)").has_value());
}

// 存储分类只把能解析出网络位置 server 的路径视为远程位置。
// 盘符路径由网络提供者判定：映射网络驱动器算远程，普通本地盘算本地。
TEST_CASE("storage kind distinguishes network paths from local paths") {
  CHECK(ClassifyPathStorageKind(LR"(\\server\share\photo.jpg)") == PathStorageKind::Remote);
  CHECK(ClassifyPathStorageKind(LR"(\\?\UNC\server\share\photo.jpg)") == PathStorageKind::Remote);
  CHECK(ClassifyPathStorageKind(LR"(C:\Photos\photo.jpg)") == PathStorageKind::Local);
  CHECK(ClassifyPathStorageKind(LR"(\\?\C:\Photos\photo.jpg)") == PathStorageKind::Local);
  CHECK(ClassifyPathStorageKind(LR"(relative\photo.jpg)") == PathStorageKind::Local);
}

// UNC 路径按语法解析；盘符路径回退到网络提供者查询映射目标。
TEST_CASE("network server resolution covers UNC syntax and mapped drive letters") {
  const auto unc_server = TryResolveNetworkServer(LR"(\\server\share\photo.jpg)");
  const auto extended_server = TryResolveNetworkServer(LR"(\\?\UNC\server\share\photo.jpg)");

  REQUIRE(unc_server.has_value());
  REQUIRE(extended_server.has_value());
  CHECK(*unc_server == L"server");
  CHECK(*extended_server == L"server");

  // 相对路径没有盘符，不需要也没有办法查询网络提供者。
  CHECK_FALSE(TryResolveNetworkServer(LR"(relative\photo.jpg)").has_value());
}

// 比较键统一 Windows 大小写、分隔符和 lexical 冗余段
TEST_CASE("comparison normalization produces a stable Windows path key") {
  CHECK(NormalizeForComparison(LR"(C:\Photos\.\Album\..\A.JPG)") == LR"(c:/photos/a.jpg)");
}

// 相同路径与任意深度的子路径都属于扫描根
TEST_CASE("path containment accepts equal and descendant paths") {
  CHECK(IsPathWithinBase(LR"(C:\Photos)", LR"(C:\Photos)"));
  CHECK(IsPathWithinBase(LR"(c:\PHOTOS)", LR"(C:\Photos)"));
  CHECK(IsPathWithinBase(LR"(C:\Photos\a.jpg)", LR"(C:\Photos)"));
  CHECK(IsPathWithinBase(LR"(C:\Photos\2026\July\a.jpg)", LR"(C:\Photos)"));
}

// 根路径和显式尾分隔符仍应保持正常的父子关系
TEST_CASE("path containment handles roots and trailing separators") {
  CHECK(IsPathWithinBase(LR"(C:\Photos\a.jpg)", LR"(C:\)"));
  CHECK(IsPathWithinBase(LR"(C:\Photos\a.jpg)", LR"(C:\Photos\)"));
}

// 路径比较必须按完整路径段判断，不能只依赖字符串前缀
TEST_CASE("path containment rejects parents siblings and similar prefixes") {
  CHECK_FALSE(IsPathWithinBase(LR"(C:\PhotoArchive\a.jpg)", LR"(C:\Photo)"));
  CHECK_FALSE(IsPathWithinBase(LR"(C:\Photos)", LR"(C:\Photos\2026)"));
  CHECK_FALSE(IsPathWithinBase(LR"(D:\Photos\a.jpg)", LR"(C:\Photos)"));
}
