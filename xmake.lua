add_rules("mode.debug", "mode.release")

set_warnings("more")

add_defines("WIN32", "_WIN32", "UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS", "_CRT_NONSTDC_NO_DEPRECATE")
set_encodings("source:utf-8")
-- set_languages("c++23")
-- xmake does not currently check for `/std:c++23preview`, but cl only supports it.
-- https://github.com/xmake-io/xmake/issues/6327
add_cxflags("/std:c++23preview", {force = true})
set_fpmodels("precise") -- Default
if is_mode("release") then
    set_exceptions("none")
    set_optimize("smallest")
    set_runtimes("MT")
    add_requires("vc-ltl5")
    add_defines("NDEBUG")
    add_ldflags("/DYNAMICBASE")
    set_policy("build.optimization.lto", true)
end

if is_mode("debug") then
    set_runtimes("MTd")
    add_defines("_DEBUG")
    add_ldflags("/DYNAMICBASE")
end

target("detours")
    set_kind("static")
    add_includedirs("detours/src", {public=true})
    add_files(
        "detours/src/detours.cpp",
        "detours/src/disasm.cpp",
        "detours/src/image.cpp",
        "detours/src/modules.cpp"
    )
    if is_arch("x86") then
        add_defines("_X86_")
        add_files("detours/src/disolx86.cpp")
    elseif is_arch("x64") then
        add_defines("_AMD64_")
        add_files("detours/src/disolx64.cpp")
    elseif is_arch("arm64") then
        add_defines("_ARM64_")
        add_files("detours/src/disolarm64.cpp")
    end
    add_cxflags("-Wno-unknown-pragmas", "-Wno-reorder-ctor", "-Wno-unused-local-typedef", {tools = "clang_cl", force = true})

target("setdll")
    set_kind("binary")
    set_targetdir("$(builddir)/$(mode)")
    add_deps("detours")
    add_files("src/*.cpp")
    add_files("src/*.rc")
    add_links("kernel32", "user32", "comctl32", "shell32", "comdlg32", "shlwapi")
    add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:wWinMainCRTStartup", {force = true})
    if is_mode("release") then
        add_packages("vc-ltl5")
    end
    after_build(function (target)
        if is_mode("release") then
            for _, file in ipairs(os.files("$(builddir)/release/*")) do
                if not file:endswith("exe") then
                    os.rm(file)
                end
            end
        end
    end)
