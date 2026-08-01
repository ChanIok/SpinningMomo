add_requires("vcpkg::doctest", "vcpkg::spdlog")

target("SpinningMomoTests")
    set_kind("binary")
    set_default(false)
    set_plat("windows")
    set_arch("x64")

    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN",
                "_WIN32_WINNT=0x0A00", "SPDLOG_COMPILED_LIB")
    add_includedirs("../src")

    add_files("../src/features/recording/time.cpp")
    add_files("../src/features/gallery/ignore/matcher.cpp")
    add_files("../src/utils/logger/logger.cpp")
    add_files("../src/utils/path/path.cpp")
    add_files("test_main.cpp")
    add_files("features/gallery/ignore/matcher_test.cpp")
    add_files("features/recording/time_test.cpp")
    add_files("utils/path_test.cpp")

    add_packages("vcpkg::doctest", "vcpkg::spdlog")
    add_links("shell32", "ole32")
    add_tests("default")

target("SpinningMomoScenarioWindow")
    set_kind("binary")
    set_default(false)
    set_plat("windows")
    set_arch("x64")

    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN",
                "_WIN32_WINNT=0x0A00")
    add_includedirs("../src")
    add_files("scenarios/window/main.cpp")
    add_links("gdi32", "shell32", "user32")
