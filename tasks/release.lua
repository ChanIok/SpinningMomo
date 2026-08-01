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
        local should_restore_debug = (config.get("mode") == "debug")
        local build_failed = false
        local build_errors

        try {
            function ()
                os.exec("xmake config -m release")
                os.exec("xmake build")
            end,
            catch {
                function (errors)
                    build_failed = true
                    build_errors = errors
                end
            },
            finally {
                function ()
                    if should_restore_debug then
                        os.exec("xmake config -m debug")
                    end
                end
            }
        }

        if build_failed then
            raise(build_errors)
        end
    end)
