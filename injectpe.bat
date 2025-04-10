@echo off

:input
set target_exe=msedge.exe
set /p target_exe=请输入要注入的程序 (默认 msedge.exe) / Enter program to inject (default msedge.exe): 

if "%target_exe%"=="" set target_exe=msedge.exe

echo %target_exe%|findstr /i ".exe" >nul
if %errorlevel% neq 0 set target_exe=%target_exe%.exe

:platform
set arch=unknown
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    set arch=x86
    setdll-x86 /t:%target_exe% 2>nul
)
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set arch=x64
    setdll-x64 /t:%target_exe% 2>nul
)
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set arch=arm64
    setdll-arm64 /t:%target_exe% 2>nul
)

if "%errorlevel%"=="32" set bits=32&goto x86
if "%errorlevel%"=="64" set bits=64&goto x64
if "%errorlevel%"=="arm64" set bits=arm64&goto arm64
goto eof

:x86
@echo ***********************************************************************
@echo *          The program will automatically inject 32-bit browsers      *
@echo *                     Press any key to continue                       *
@echo ***********************************************************************
@echo *                     程序自动注入32位的浏览器                        *
@echo *                          按任意键继续                               *
@echo ***********************************************************************
echo+
@pause .
goto runing

:x64
echo+
@echo ***********************************************************************
@echo *          The program will automatically inject 64-bit browsers      *
@echo *                     Press any key to continue                       *
@echo ***********************************************************************
@echo *                     程序自动注入64位的浏览器                        *
@echo *                          按任意键继续                               *
@echo ***********************************************************************
echo+
@pause .
goto runing

:arm64
echo+
@echo ***********************************************************************
@echo *         The program will automatically inject ARM64 browsers        *
@echo *                     Press any key to continue                       *
@echo ***********************************************************************
@echo *                    程序自动注入ARM64的浏览器                        *
@echo *                          按任意键继续                               *
@echo ***********************************************************************
echo+
@pause .

:runing
setdll-%arch% /d:version-%arch%.dll %target_exe% 2>nul
if "%errorlevel%"=="0" echo 成功！(Done) &goto eof
echo 失败！(Fail)

:eof
pause .
@del /s/q setdll-*.exe 2>nul 1>nul
if "%arch%"=="x86" del /s/q version-x64.dll version-arm64.dll 2>nul 1>nul
if "%arch%"=="x64" del /s/q version-x86.dll version-arm64.dll 2>nul 1>nul
if "%arch%"=="arm64" del /s/q version-x86.dll version-x64.dll 2>nul 1>nul
@del /q %0 2>nul 1>nul