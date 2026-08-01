#include "features/gallery/ignore/matcher.hpp"

#include "vendor/doctest.hpp"

namespace features::gallery::ignore::matcher {

TEST_CASE("glob matches the complete root-relative path") {
  CHECK(match_glob_pattern("*.jpg", "photo.jpg"));
  CHECK_FALSE(match_glob_pattern("*.jpg", "photos/photo.jpg"));

  CHECK(match_glob_pattern("photos/*", "photos/photo.jpg"));
  CHECK_FALSE(match_glob_pattern("photos/*", "photos/2026/photo.jpg"));
}

TEST_CASE("globstar crosses complete path segments") {
  CHECK(match_glob_pattern("**/*.jpg", "photo.jpg"));
  CHECK(match_glob_pattern("**/*.jpg", "photos/2026/photo.jpg"));

  CHECK(match_glob_pattern("a/**/b.jpg", "a/b.jpg"));
  CHECK(match_glob_pattern("a/**/b.jpg", "a/x/y/b.jpg"));
  CHECK_FALSE(match_glob_pattern("a/**/b.jpg", "a/xxb.jpg"));

  CHECK(match_glob_pattern("photos/**", "photos/2026/photo.jpg"));
  CHECK_FALSE(match_glob_pattern("photos/**", "photos"));
}

TEST_CASE("glob supports wildcards and character classes") {
  CHECK(match_glob_pattern("photo?.jpg", "photo1.jpg"));
  CHECK_FALSE(match_glob_pattern("photo?.jpg", "photo10.jpg"));

  CHECK(match_glob_pattern("[0-9]*.jpg", "1-photo.jpg"));
  CHECK_FALSE(match_glob_pattern("[0-9]*.jpg", "a-photo.jpg"));
  CHECK(match_glob_pattern("[!0-9]*.jpg", "a-photo.jpg"));
  CHECK_FALSE(match_glob_pattern("[!0-9]*.jpg", "1-photo.jpg"));
}

TEST_CASE("glob matching is case insensitive on Windows") {
  CHECK(match_glob_pattern("Photos/*.JPG", "photos/photo.jpg"));
}

}  // namespace features::gallery::ignore::matcher
