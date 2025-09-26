-- 构建release版本并智能恢复配置
task("release")
    set_menu {
        usage = "xmake release",
        description = "Build in release mode and auto restore debug config"
    }
    
    on_run(function ()
        import("core.project.config")
        
        -- 获取当前配置状态
        config.load()
        local current_mode = config.get("mode")
        local should_restore = (current_mode == "debug")
        
        -- 构建release版本
        os.exec("xmake config -m release")
        local ok = os.exec("xmake build")
        
        if ok == 0 and should_restore then
            os.exec("xmake config -m debug")
        end
        
    end)
