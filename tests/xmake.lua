add_requires("vcpkg::doctest")

target("SpinningMomoTests")
    set_kind("binary")
    set_default(false)
    set_plat("windows")
    set_arch("x64")

    -- 测试目标只编译当前被测模块，避免重复构建整个应用
    set_policy("build.c++.modules", true)
    add_files("../src/features/recording/time.ixx")
    add_files("../src/features/recording/time.cpp")
    add_files("../src/utils/path/path.ixx")
    add_files("../src/utils/path/path.cpp")
    add_files("../src/vendor/shellapi.ixx")
    add_files("test_main.cpp")
    add_files("features/recording/time_test.cpp")
    add_files("utils/path_test.cpp")

    add_packages("vcpkg::doctest")
    add_links("shell32", "ole32")
    add_tests("default")
