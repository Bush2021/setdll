includes("VC-LTL5.lua", "YY-Thunks.lua")

add_rules("mode.debug", "mode.release")

set_warnings("more")
add_defines("WIN32", "_WIN32")
add_defines("UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS", "_CRT_NONSTDC_NO_DEPRECATE")

if is_mode("release") then
    add_defines("NDEBUG")
    add_cxflags("/O2", "/Os", "/Gy", "/MT", "/EHsc", "/fp:precise")
    add_ldflags("/DYNAMICBASE", "/LTCG")
end

add_cxflags("/utf-8")
add_links("kernel32", "user32")

target("detours")
    set_kind("static")
    add_files("detours/src/*.cpp|uimports.cpp")
    add_includedirs("detours/src", {public=true})

target("setdll")
    set_kind("binary")
    set_targetdir("$(buildir)/release")
    add_deps("detours")
    add_links("detours")
    add_files("src/*.cpp")