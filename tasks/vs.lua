-- 生成Visual Studio项目文件
task("vs")
    set_menu {
        usage = "xmake vs",
        description = "Generate Visual Studio project files for debug and release modes"
    }
    
    on_run(function ()
        print("Generating Visual Studio project files...")
        os.exec("xmake project -k vsxmake -m \"debug,release\"")
        print("Visual Studio project files generated successfully!")
    end)