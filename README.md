## Set DLL for [chrome_plus](https://github.com/Bush2021/chrome_plus)

A utility tool for DLL injection into Windows executables, based on Microsoft Detours.

### Usage

```
setdll [options] binary_files
```

Options:
* `/d:file.dll` : Inject specified DLL into target binaries
* `/r` : Remove extra DLLs from binaries
* `/?` : Display help information

### Injection Script

* Extract all files from this [archive](https://github.com/Bush2021/chrome_plus/releases/download/latest/setdll.7z) to the directory where the browser executable to be injected is located, and run `injectpe.bat`. Once the command line runs successfully, it will automatically delete the extra files.

* `injectpe.bat` provides a convenient wrapper for injection operations. The default target is Microsoft Edge browser. If you want to use it on other browsers, please modify the `msedge.exe` field in `injectpe.bat` to the executable of the desired browser.


### License

This project is licensed under GPL-3.0. 
Original project under MIT license.

### References:
- Original project: https://github.com/adonais/setdll
- Microsoft Detours: https://github.com/microsoft/Detours
