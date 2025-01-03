## Set DLL for [chrome_plus](https://github.com/Bush2021/chrome_plus)
* Extract all files from this [archive](https://github.com/Bush2021/chrome_plus/releases/download/latest/setdll.7z) to the directory where the browser executable to be injected is located, and run `injectpe.bat`. Once the command line runs successfully, it will automatically delete the extra files.

* `injectpe.bat` is configured for Microsoft Edge. If you want to use it on other browsers, please modify the `msedge.exe` field in `injectpe.bat` to the executable of the desired browser.

* Original project: https://github.com/adonais/setdll/

## Original Project Description
### System Requirements

* C compiler
* Microsoft Visual Studio 2013 or above

### Build

```sh
nmake clean
nmake
```

or

```sh
nmake CC=clang-cl clean
nmake CC=clang-cl
```

### About setdll

**Usage:**
```
setdll [options] binary_files
```

**Options:**

* `/d:file.dll` : Add `file.dll` to binary files
* `/r` : Remove extra DLLs from binary files
* `/p:browser\omni.ja` : Repair `omni.ja` to support `Upcheck.exe`
* `/t:file.exe` : Test PE file bits
* `/?` : Display this help screen
* `-7 --help` : Display 7z command help screen

