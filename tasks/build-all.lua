-- 构建完整的发布版本（包含Web应用）
task("build-all")
    set_menu {
        usage = "xmake build-all",
        description = "Build release version with web app"
    }
    
    on_run(function ()
        import("core.project.config")
        import("core.project.project")
        
        -- 1. 构建release版本
        print("Building release version...")
        os.exec("xmake config -m release")
        os.exec("xmake build")
        
        -- 2. 构建web应用
        print("Building web app...")
        local old_dir = os.curdir()
        os.cd("web")
        
        if os.host() == "windows" then
            os.exec("npm.cmd run build")
        else
            os.exec("npm run build")
        end
        
        os.cd(old_dir)
        
        -- 3. 复制web资源
        print("Copying web resources...")
        local web_dist = path.join(os.projectdir(), "web/dist")
        config.load()
        local target = project.target("SpinningMomo")
        local outputdir = target:targetdir()
        local web_target = path.join(outputdir, "resources/web")
        
        os.mkdir(path.join(outputdir, "resources"))
        os.cp(web_dist .. "/*", web_target)
        
        print("Build completed: " .. outputdir)
    end)
