target("YY-Thunks")
    set_kind("phony")
    before_build("windows", function (target)
        local function find_in_file()
            for _, dir in ipairs(os.dirs("$(projectdir)/*")) do
                name = dir:match(".*\\(.*)")
                if name:find("YY%-Thunks") then
                    return dir .. [[\]]
                end
            end
        end
        local function find_in_reg()
            return vformat("$(reg HKEY_CURRENT_USER\\Code\\YY-Thunks;Root)")
        end
        local YY_Thunks_Root = find_in_file() or find_in_reg()
        if #YY_Thunks_Root == 0 then
            return
        end

        local WindowsTargetPlatformMinVersion = "5.1.2600.0"
        
        cprint("${color.warning}YY-Thunks Path : %s", YY_Thunks_Root)
        cprint("${color.warning}WindowsTargetPlatformMinVersion : %s", WindowsTargetPlatformMinVersion)
        
        import("core.tool.toolchain")
        local msvc = toolchain.load("msvc")
        local runenvs = msvc:runenvs()

        local arch = target:arch()
        local archpath = "Win32"
        if arch ~= "x86" then
            archpath = arch
        end
        cprint("${color.warning}Platform : %s", archpath)

        local libpath = YY_Thunks_Root .. [[Lib\]] .. WindowsTargetPlatformMinVersion .. [[\]] .. archpath .. ";"
        runenvs.LIB = libpath .. runenvs.LIB

        if target:kind() == "shared" then
            target:add("ldflags", "/ENTRY:DllMainCRTStartupForYY_Thunks", {force = true})
        end
        
        if archpath == "x86" then
            target:add("ldflags", "/SUBSYSTEM:WINDOWS,5.01", {force = true})
        else
            target:add("ldflags", "/SUBSYSTEM:WINDOWS,5.02", {force = true})
        end
    end)