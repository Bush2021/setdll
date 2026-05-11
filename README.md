## Set DLL for [chrome_plus](https://github.com/Bush2021/chrome_plus)

A utility tool for DLL injection into Windows executables, based on Microsoft Detours.

### Quick start (GUI)

1. Extract all files from [setdll.7z](https://github.com/Bush2021/chrome_plus/releases/latest) to the directory where the browser executable is located.
2. Drag the browser executable (e.g., `msedge.exe`) onto `setdll.exe`, or double-click `setdll.exe` and pick the target with **Browse...**.
3. The dialog auto-detects the target's architecture, looks for `version-<arch>.dll` next to the target, and offers **Inject** or **Restore**. Click — done.

The auto-detected DLL name (`version-<arch>.dll`) is the chrome_plus default. If you want to inject a different DLL, click **Change...** on the DLL row to pick any `.dll` instead. For scripted or batch use, see the command line below.

### Command line

```
setdll [options] binary_files
```

Options:
* `/d:file.dll` — inject the specified DLL (basename or full path both work)
* `/r` — remove all injected DLLs from the target
* `/t:file.exe` — print PE architecture information; exits 0 on success, 1 if the file is not a recognized PE
* `/?` — help

Examples:

```
setdll /d:mymod-x64.dll target.exe
setdll /r target.exe
setdll /t:target.exe
```

`setdll.exe` is a Windows-subsystem binary; CLI output attaches to the parent console (`cmd.exe`, PowerShell, etc.).

> **v3 breaking change**: `/t:` now exits `0` on success and `1` on failure. Previous versions returned the PE machine word as the exit code (`332` / `34404` / `43620`); scripts that branched on `%errorlevel%` to dispatch on architecture need to parse stdout instead. Other CLI options are unchanged.

### License

This project is licensed under GPL-3.0. 
Original project under MIT license.

### References:
- Original project: https://github.com/adonais/setdll
- Microsoft Detours: https://github.com/microsoft/Detours
