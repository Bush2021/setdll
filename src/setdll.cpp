//////////////////////////////////////////////////////////////////////////////
//
//  Detours Test Program (setdll.cpp of setdll.exe)
//
//  Microsoft Research Detours Package
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <shellapi.h>
#include <detours.h>
#pragma warning(push)
#if _MSC_VER > 1400
#pragma warning(disable:6102 6103) // /analyze warnings
#endif
#include <strsafe.h>
#define is_valid_handle(x) (x != NULL && x != INVALID_HANDLE_VALUE)
#pragma warning(pop)
#include <commctrl.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "setdll_resources.h"

////////////////////////////////////////////////////////////// Error Messages.
//
VOID AssertMessage(PCSTR szMsg, PCSTR szFile, DWORD nLine)
{
    printf("ASSERT(%s) failed in %s, line %ld.", szMsg, szFile, nLine);
}

#define ASSERT(x)   \
do { if (!(x)) { AssertMessage(#x, __FILE__, __LINE__); DebugBreak(); }} while (0)
    ;


//////////////////////////////////////////////////////////////////////////////
//
static BOOLEAN  s_fRemove = FALSE;
static BOOLEAN  s_fShowPEInfo = FALSE;
static CHAR     s_szDllPath[MAX_PATH] = "";
static WCHAR    w_szDllPath[MAX_PATH] = L"";

//////////////////////////////////////////////////////////////////////////////
//
//  This code verifies that the named DLL has been configured correctly
//  to be imported into the target process.  DLLs must export a function with
//  ordinal #1 so that the import table touch-up magic works.
//
static BOOL CALLBACK ExportCallback(_In_opt_ PVOID pContext,
                                    _In_ ULONG nOrdinal,
                                    _In_opt_ LPCSTR pszName,
                                    _In_opt_ PVOID pCode)
{
    (void)pContext;
    (void)pCode;
    (void)pszName;

    if (nOrdinal == 1) {
        *((BOOL *)pContext) = TRUE;
    }
    return TRUE;
}

BOOL DoesDllExportOrdinal1(PCHAR pszDllPath)
{
    HMODULE hDll = LoadLibraryExA(pszDllPath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (hDll == NULL) {
        printf("setdll.exe: LoadLibraryEx(%s) failed with error %ld.\n",
               pszDllPath,
               GetLastError());
        return FALSE;
    }

    BOOL validFlag = FALSE;
    DetourEnumerateExports(hDll, &validFlag, ExportCallback);
    FreeLibrary(hDll);
    return validFlag;
}

//////////////////////////////////////////////////////////////////////////////
//
static BOOL CALLBACK ListBywayCallback(_In_opt_ PVOID pContext,
                                       _In_opt_ LPCSTR pszFile,
                                       _Outptr_result_maybenull_ LPCSTR *ppszOutFile)
{
    (void)pContext;

    *ppszOutFile = pszFile;
    if (pszFile) {
        printf("    %s\n", pszFile);
    }
    return TRUE;
}

static BOOL CALLBACK ListFileCallback(_In_opt_ PVOID pContext,
                                      _In_ LPCSTR pszOrigFile,
                                      _In_ LPCSTR pszFile,
                                      _Outptr_result_maybenull_ LPCSTR *ppszOutFile)
{
    (void)pContext;

    *ppszOutFile = pszFile;
    printf("    %s -> %s\n", pszOrigFile, pszFile);
    return TRUE;
}

static BOOL CALLBACK AddBywayCallback(_In_opt_ PVOID pContext,
                                      _In_opt_ LPCSTR pszFile,
                                      _Outptr_result_maybenull_ LPCSTR *ppszOutFile)
{
    PBOOL pbAddedDll = (PBOOL)pContext;
    if (!pszFile && !*pbAddedDll) {                     // Add new byway.
        *pbAddedDll = TRUE;
        *ppszOutFile = s_szDllPath;
    }
    return TRUE;
}

BOOL SetFile(PCHAR pszPath)
{
    BOOL bGood = TRUE;
    HANDLE hOld = INVALID_HANDLE_VALUE;
    HANDLE hNew = INVALID_HANDLE_VALUE;
    PDETOUR_BINARY pBinary = NULL;

    CHAR szOrg[MAX_PATH];
    CHAR szNew[MAX_PATH];
    CHAR szOld[MAX_PATH];

    szOld[0] = '\0';
    szNew[0] = '\0';

    StringCchCopyA(szOrg, sizeof(szOrg), pszPath);
    StringCchCopyA(szNew, sizeof(szNew), szOrg);
    StringCchCatA(szNew, sizeof(szNew), "#");
    StringCchCopyA(szOld, sizeof(szOld), szOrg);
    StringCchCatA(szOld, sizeof(szOld), "~");
    printf("  %s:\n", pszPath);

    hOld = CreateFileA(szOrg,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);

    if (hOld == INVALID_HANDLE_VALUE) {
        printf("Couldn't open input file: %s, error: %ld\n",
               szOrg, GetLastError());
        bGood = FALSE;
        goto end;
    }

    hNew = CreateFileA(szNew,
                       GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hNew == INVALID_HANDLE_VALUE) {
        printf("Couldn't open output file: %s, error: %ld\n",
               szNew, GetLastError());
        bGood = FALSE;
        goto end;
    }

    if ((pBinary = DetourBinaryOpen(hOld)) == NULL) {
        printf("DetourBinaryOpen failed: %ld\n", GetLastError());
        goto end;
    }

    if (hOld != INVALID_HANDLE_VALUE) {
        CloseHandle(hOld);
        hOld = INVALID_HANDLE_VALUE;
    }

    {
        BOOL bAddedDll = FALSE;

        DetourBinaryResetImports(pBinary);

        if (!s_fRemove) {
            if (!DetourBinaryEditImports(pBinary,
                                         &bAddedDll,
                                         AddBywayCallback, NULL, NULL, NULL)) {
                printf("DetourBinaryEditImports failed: %ld\n", GetLastError());
            }
        }

        if (!DetourBinaryEditImports(pBinary, NULL,
                                     ListBywayCallback, ListFileCallback,
                                     NULL, NULL)) {

            printf("DetourBinaryEditImports failed: %ld\n", GetLastError());
        }

        if (!DetourBinaryWrite(pBinary, hNew)) {
            printf("DetourBinaryWrite failed: %ld\n", GetLastError());
            bGood = FALSE;
        }

        DetourBinaryClose(pBinary);
        pBinary = NULL;

        if (hNew != INVALID_HANDLE_VALUE) {
            CloseHandle(hNew);
            hNew = INVALID_HANDLE_VALUE;
        }

        if (bGood) {
            if (!DeleteFileA(szOld)) {
                DWORD dwError = GetLastError();
                if (dwError != ERROR_FILE_NOT_FOUND) {
                    printf("Warning: Couldn't delete %s: %ld\n", szOld, dwError);
                    bGood = FALSE;
                }
            }
            if (!MoveFileA(szOrg, szOld)) {
                printf("Error: Couldn't back up %s to %s: %ld\n",
                       szOrg, szOld, GetLastError());
                bGood = FALSE;
            }
            if (!MoveFileA(szNew, szOrg)) {
                printf("Error: Couldn't install %s as %s: %ld\n",
                       szNew, szOrg, GetLastError());
                bGood = FALSE;
            }
        }

        DeleteFileA(szNew);
    }


  end:
    if (pBinary) {
        DetourBinaryClose(pBinary);
        pBinary = NULL;
    }
    if (hNew != INVALID_HANDLE_VALUE) {
        CloseHandle(hNew);
        hNew = INVALID_HANDLE_VALUE;
    }
    if (hOld != INVALID_HANDLE_VALUE) {
        CloseHandle(hOld);
        hOld = INVALID_HANDLE_VALUE;
    }
    return bGood;
}

//////////////////////////////////////////////////////////////////////////////
//
WORD __stdcall
get_pe_machine(const wchar_t* path)
{
    HANDLE hFile = CreateFileW(path, GENERIC_READ,
                               FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (!is_valid_handle(hFile))
    {
        return IMAGE_FILE_MACHINE_UNKNOWN;
    }

    IMAGE_DOS_HEADER dos_header;
    IMAGE_NT_HEADERS pe_header;
    WORD machine = IMAGE_FILE_MACHINE_UNKNOWN;
    do
    {
        DWORD readed = 0;
        if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        {
            break;
        }
        if (!ReadFile(hFile, &dos_header, sizeof(dos_header), &readed, NULL) ||
            readed != sizeof(dos_header) ||
            dos_header.e_magic != IMAGE_DOS_SIGNATURE)
        {
            break;
        }
        if (SetFilePointer(hFile, dos_header.e_lfanew, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        {
            break;
        }
        if (!ReadFile(hFile, &pe_header, sizeof(pe_header), &readed, NULL) ||
            readed != sizeof(pe_header))
        {
            break;
        }
        machine = pe_header.FileHeader.Machine;
    } while (0);
    CloseHandle(hFile);
    return machine;
}

//////////////////////////////////////////////////////////////////////////////
//
//  GUI helpers — architecture detection, DLL discovery, patched-state check.
//

enum class TargetArch { Unknown, X86, X64, ARM64 };

static TargetArch DetectTargetArch(const wchar_t* path)
{
    switch (get_pe_machine(path)) {
        case IMAGE_FILE_MACHINE_I386:  return TargetArch::X86;
        case IMAGE_FILE_MACHINE_IA64:
        case IMAGE_FILE_MACHINE_AMD64: return TargetArch::X64;
        case IMAGE_FILE_MACHINE_ARM64: return TargetArch::ARM64;
        default:                       return TargetArch::Unknown;
    }
}

static const wchar_t* ArchName(TargetArch a)
{
    switch (a) {
        case TargetArch::X86:   return L"x86";
        case TargetArch::X64:   return L"x64";
        case TargetArch::ARM64: return L"arm64";
        default:                return L"unknown";
    }
}

static std::string WideToAcp(std::wstring_view w)
{
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_ACP, 0, w.data(), static_cast<int>(w.size()),
                                      nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_ACP, 0, w.data(), static_cast<int>(w.size()),
                        out.data(), n, nullptr, nullptr);
    return out;
}

static std::optional<std::filesystem::path>
FindBywayDll(const std::filesystem::path& targetDir,
             const std::filesystem::path& exeDir,
             const wchar_t* archName)
{
    const std::wstring dllName = std::wstring(L"version-") + archName + L".dll";
    for (const std::filesystem::path& dir : { targetDir, exeDir }) {
        auto candidate = dir / dllName;
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return std::nullopt;
}

struct PatchedCtx { const char* needle; BOOL found; };

static BOOL CALLBACK PatchedCheckCallback(_In_opt_ PVOID pContext,
                                          _In_opt_ LPCSTR pszFile,
                                          _Outptr_result_maybenull_ LPCSTR* ppszOutFile)
{
    PatchedCtx* ctx = (PatchedCtx*)pContext;
    *ppszOutFile = pszFile;
    if (pszFile && ctx && ctx->needle) {
        const char* pszBasename = PathFindFileNameA(pszFile);
        if (_stricmp(pszBasename, ctx->needle) == 0) {
            ctx->found = TRUE;
        }
    }
    return TRUE;
}

static BOOL IsAlreadyPatched(const char* targetPath, const char* dllName)
{
    HANDLE hFile = CreateFileA(targetPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    PDETOUR_BINARY pBinary = DetourBinaryOpen(hFile);
    CloseHandle(hFile);
    if (!pBinary) return FALSE;

    PatchedCtx ctx = { dllName, FALSE };
    DetourBinaryEditImports(pBinary, &ctx, PatchedCheckCallback, NULL, NULL, NULL);
    DetourBinaryClose(pBinary);
    return ctx.found;
}

//////////////////////////////////////////////////////////////////////////////
//
void PrintUsage(void)
{
    printf("Usage:\n"
           "    setdll [options] binary_files\n"
           "Options:\n"
           "    /d:file.dll  : Inject specified DLL into target binaries\n"
           "    /r           : Remove extra DLLs from binaries\n"
           "    /t:file.exe  : Show PE file architecture information\n"
           "    /?           : Display help information\n");
}

//////////////////////////////////////////////////////////////////////// main.
//
static int cli_main(int argc, char **argv)
{
    BOOL fNeedHelp = FALSE;
    PCHAR pszFilePart = NULL;

    int arg = 1;
    for (; arg < argc; arg++) {
        if (argv[arg][0] == '-' || argv[arg][0] == '/') {
            CHAR *argn = argv[arg] + 1;
            CHAR *argp = argn;
            while (*argp && *argp != ':' && *argp != '=')
                argp++;
            if (*argp == ':' || *argp == '=')
                *argp++ = '\0';

            switch (argn[0]) {

              case 'd':                                 // Set DLL
              case 'D':
                if ((strchr(argp, ':') != NULL || strchr(argp, '\\') != NULL) &&
                    GetFullPathNameA(argp, sizeof(s_szDllPath), s_szDllPath, &pszFilePart)) {
                }
                else {
                    StringCchPrintfA(s_szDllPath, sizeof(s_szDllPath), "%s", argp);
                }
                break;

              case 'r':                                 // Remove extra set DLLs.
              case 'R':
                s_fRemove = TRUE;
                break;

                case 't': // get PE file bits
                case 'T':
                    s_fShowPEInfo = TRUE;
                    if (strlen(argp) < 1)
                    {
                        fNeedHelp = TRUE;
                        break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, argp, -1, w_szDllPath, MAX_PATH);

                    if (argp[0] != ':' && strchr(argp, '\\') != NULL)
                    {
                        WCHAR fullPath[MAX_PATH];
                        if (GetFullPathNameW(w_szDllPath, MAX_PATH, fullPath, NULL)) {
                            wcscpy_s(w_szDllPath, MAX_PATH, fullPath);
                        }
                    }

                    WideCharToMultiByte(CP_ACP, 0, w_szDllPath, -1, s_szDllPath, sizeof(s_szDllPath), NULL, NULL);

                    if (s_szDllPath[0] != '\0')
                    {
                        switch (DetectTargetArch(w_szDllPath)) {
                            case TargetArch::X86:
                                printf("PE32 executable (i386), for MS Windows\n");
                                return 0;
                            case TargetArch::X64:
                                printf("PE32+ executable (x86-64), for MS Windows\n");
                                return 0;
                            case TargetArch::ARM64:
                                printf("PE32+ executable (ARM64), for MS Windows\n");
                                return 0;
                            default:
                                printf("Unknown PE format or not a valid PE file\n");
                                return 1;
                        }
                    }
                    break;

              case '?':                                 // Help
                fNeedHelp = TRUE;
                break;

              default:
                fNeedHelp = TRUE;
                printf("Bad argument: %s:%s\n", argn, argp);
                break;
            }
        }
    }
    if (argc == 1) {
        fNeedHelp = TRUE;
    }
    if (!s_fRemove && s_szDllPath[0] == 0 && !s_fShowPEInfo) {
        fNeedHelp = TRUE;
    }
    if (fNeedHelp) {
        PrintUsage();
        return 1;
    }

    if (s_fShowPEInfo) {
        return 0;
    }

    if (s_fRemove) {
        printf("Removing extra DLLs from binary files.\n");
    }
    else {
        if (!DoesDllExportOrdinal1(s_szDllPath)) {
            printf("Error: %hs does not export function with ordinal #1.\n",
                   s_szDllPath);
            return 2;
        }
        printf("Adding %hs to binary files.\n", s_szDllPath);
    }

    for (arg = 1; arg < argc; arg++) {
        if (argv[arg][0] != '-' && argv[arg][0] != '/') {
            SetFile(argv[arg]);
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
//  GUI dispatcher.
//

static int RunInjectGui(HINSTANCE hInstance, const wchar_t* preloadedTarget);

static void AttachParentConsole(void)
{
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$",  "r", stdin);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//  GUI dialog state and procedure.
//

struct GuiState {
    std::wstring targetPath;
    std::wstring dllPath;
    TargetArch arch        = TargetArch::Unknown;
    bool targetLoaded      = false;
    bool alreadyPatched    = false;
    bool dllFound          = false;
};

static void RenderEmpty(HWND hDlg)
{
    SetDlgItemTextW(hDlg, IDC_PROMPT, L"Drop a Windows executable here, or browse:");
    ShowWindow(GetDlgItem(hDlg, IDC_BROWSE),       SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_TARGET_GROUP), SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_FILE),   SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_FILE),    SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_ARCH),   SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_ARCH),    SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_DLL),    SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_DLL),     SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_BROWSE_DLL),   SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_STATUS), SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_STATUS),  SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_PRIMARY),      SW_HIDE);
}

static void RenderLoaded(HWND hDlg, const GuiState* s)
{
    SetDlgItemTextW(hDlg, IDC_PROMPT, L"Drop a different file to swap targets, or:");
    ShowWindow(GetDlgItem(hDlg, IDC_BROWSE),       SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_TARGET_GROUP), SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_FILE),   SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_FILE),    SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_ARCH),   SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_ARCH),    SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_DLL),    SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_DLL),     SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_BROWSE_DLL),   SW_SHOW);
    EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_DLL), TRUE);
    ShowWindow(GetDlgItem(hDlg, IDC_LABEL_STATUS), SW_SHOW);
    ShowWindow(GetDlgItem(hDlg, IDC_TEXT_STATUS),  SW_SHOW);

    SetDlgItemTextW(hDlg, IDC_TEXT_FILE, s->targetPath.c_str());
    SetDlgItemTextW(hDlg, IDC_TEXT_ARCH, ArchName(s->arch));
    SetDlgItemTextW(hDlg, IDC_TEXT_DLL,
                    s->dllFound ? s->dllPath.c_str() : L"(not found next to target or setdll.exe)");

    if (!s->dllFound) {
        SetDlgItemTextW(hDlg, IDC_TEXT_STATUS,
                        L"Cannot proceed: place version-<arch>.dll next to the target.");
        EnableWindow(GetDlgItem(hDlg, IDC_PRIMARY), FALSE);
        SetDlgItemTextW(hDlg, IDC_PRIMARY, L"Inject");
    } else if (s->alreadyPatched) {
        SetDlgItemTextW(hDlg, IDC_TEXT_STATUS, L"Already injected. Click Restore to remove.");
        EnableWindow(GetDlgItem(hDlg, IDC_PRIMARY), TRUE);
        SetDlgItemTextW(hDlg, IDC_PRIMARY, L"Restore");
    } else {
        SetDlgItemTextW(hDlg, IDC_TEXT_STATUS, L"Not yet injected.");
        EnableWindow(GetDlgItem(hDlg, IDC_PRIMARY), TRUE);
        SetDlgItemTextW(hDlg, IDC_PRIMARY, L"Inject");
    }
    ShowWindow(GetDlgItem(hDlg, IDC_PRIMARY), SW_SHOW);
}

static void RenderResult(HWND hDlg, BOOL ok, const wchar_t* message)
{
    SetDlgItemTextW(hDlg, IDC_TEXT_STATUS,
                    message ? message : (ok ? L"Done." : L"Failed."));
    SetDlgItemTextW(hDlg, IDC_PRIMARY, L"OK");
    EnableWindow(GetDlgItem(hDlg, IDC_PRIMARY),    TRUE);
    EnableWindow(GetDlgItem(hDlg, IDC_BROWSE),     FALSE);
    EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_DLL), FALSE);
}

static void RecheckPatched(GuiState* s)
{
    if (!s->targetLoaded || !s->dllFound) {
        s->alreadyPatched = false;
        return;
    }
    const std::string targetA  = WideToAcp(s->targetPath);
    const std::string dllNameA =
        WideToAcp(std::filesystem::path(s->dllPath).filename().wstring());
    s->alreadyPatched = IsAlreadyPatched(targetA.c_str(), dllNameA.c_str()) != FALSE;
}

static void LoadTarget(HWND hDlg, GuiState* s, const std::wstring& path)
{
    *s = GuiState{};
    s->targetPath = path;
    s->targetLoaded = true;

    s->arch = DetectTargetArch(path.c_str());
    if (s->arch == TargetArch::Unknown) {
        RenderLoaded(hDlg, s);
        SetDlgItemTextW(hDlg, IDC_TEXT_STATUS, L"Not a valid Windows executable.");
        EnableWindow(GetDlgItem(hDlg, IDC_PRIMARY), FALSE);
        return;
    }

    const std::filesystem::path targetDir = std::filesystem::path(path).parent_path();

    wchar_t exePathBuf[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    const std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();

    if (auto dll = FindBywayDll(targetDir, exeDir, ArchName(s->arch))) {
        s->dllFound = true;
        s->dllPath  = dll->wstring();
    }
    RecheckPatched(s);

    RenderLoaded(hDlg, s);
}

static std::pair<bool, std::wstring> DoPatchOrRestore(GuiState* s, bool remove)
{
    if (!s->targetLoaded || !s->dllFound) {
        return { false, L"Internal error: target or DLL not set." };
    }

    s_fRemove = remove ? TRUE : FALSE;
    if (!remove) {
        std::string fullDllA = WideToAcp(s->dllPath);
        if (!DoesDllExportOrdinal1(fullDllA.data())) {
            return { false, L"Selected DLL is missing ordinal #1 export." };
        }
        const std::string dllBasenameA =
            WideToAcp(std::filesystem::path(s->dllPath).filename().wstring());
        StringCchCopyA(s_szDllPath, sizeof(s_szDllPath), dllBasenameA.c_str());
    } else {
        s_szDllPath[0] = '\0';
    }

    std::string targetA = WideToAcp(s->targetPath);

    FILE* nul = nullptr;
    freopen_s(&nul, "NUL", "w", stdout);

    SetLastError(ERROR_SUCCESS);
    const BOOL ok = SetFile(targetA.data());
    const DWORD err = GetLastError();

    if (nul) fclose(nul);

    const std::wstring targetName = std::filesystem::path(s->targetPath).filename().wstring();
    const std::wstring dllName    = std::filesystem::path(s->dllPath).filename().wstring();

    if (!ok) {
        if (err == ERROR_SHARING_VIOLATION) {
            return { false, targetName + L" is in use. Close all instances and try again." };
        }
        if (err == ERROR_ACCESS_DENIED) {
            return { false, L"Access denied. Try running as administrator." };
        }
        wchar_t buf[64];
        StringCchPrintfW(buf, 64, L"Patching failed (Win32 error %lu).", err);
        return { false, buf };
    }

    return { true,
             L"Done. " + targetName +
             (remove ? L" no longer loads " : L" now loads ") +
             dllName + L" on launch." };
}

static INT_PTR CALLBACK InjectDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static GuiState s_state;
    enum class Phase { Empty, Loaded, Result };
    static Phase s_phase = Phase::Empty;

    switch (msg) {
    case WM_INITDIALOG: {
        DragAcceptFiles(hDlg, TRUE);
        s_state = GuiState{};

        const wchar_t* preload = reinterpret_cast<const wchar_t*>(lParam);
        if (preload && preload[0] && std::filesystem::exists(preload)) {
            s_phase = Phase::Loaded;
            LoadTarget(hDlg, &s_state, preload);
        } else {
            s_phase = Phase::Empty;
            RenderEmpty(hDlg);
        }
        return TRUE;
    }

    case WM_DROPFILES: {
        if (s_phase == Phase::Result) return TRUE;
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        wchar_t path[MAX_PATH];
        UINT n = DragQueryFileW(hDrop, 0, path, MAX_PATH);
        DragFinish(hDrop);
        if (n > 0 && std::filesystem::exists(path)) {
            s_phase = Phase::Loaded;
            LoadTarget(hDlg, &s_state, path);
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BROWSE: {
            wchar_t buf[MAX_PATH] = L"";
            OPENFILENAMEW ofn = { sizeof(ofn) };
            ofn.hwndOwner   = hDlg;
            ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All files (*.*)\0*.*\0";
            ofn.lpstrFile   = buf;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn)) {
                s_phase = Phase::Loaded;
                LoadTarget(hDlg, &s_state, buf);
            }
            return TRUE;
        }
        case IDC_BROWSE_DLL: {
            if (!s_state.targetLoaded) return TRUE;
            wchar_t buf[MAX_PATH] = L"";
            const std::wstring initialDir =
                std::filesystem::path(s_state.targetPath).parent_path().wstring();
            OPENFILENAMEW ofn = { sizeof(ofn) };
            ofn.hwndOwner       = hDlg;
            ofn.lpstrFilter     = L"DLL files (*.dll)\0*.dll\0All files (*.*)\0*.*\0";
            ofn.lpstrFile       = buf;
            ofn.nMaxFile        = MAX_PATH;
            ofn.lpstrInitialDir = initialDir.c_str();
            ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn)) {
                s_state.dllPath  = buf;
                s_state.dllFound = true;
                RecheckPatched(&s_state);
                RenderLoaded(hDlg, &s_state);
            }
            return TRUE;
        }
        case IDC_PRIMARY: {
            if (s_phase == Phase::Result) {
                EndDialog(hDlg, 0);
                return TRUE;
            }
            auto [ok, msg] = DoPatchOrRestore(&s_state, s_state.alreadyPatched);
            s_phase = Phase::Result;
            RenderResult(hDlg, ok, msg.c_str());
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, 1);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, 1);
        return TRUE;
    }

    return FALSE;
}

static int RunInjectGui(HINSTANCE hInstance, const wchar_t* preloadedTarget)
{
    INT_PTR rc = DialogBoxParamW(hInstance,
                                 MAKEINTRESOURCEW(IDD_INJECT_DIALOG),
                                 NULL,
                                 InjectDialogProc,
                                 (LPARAM)preloadedTarget);
    return (rc < 0) ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////////
//
extern "C" int APIENTRY wWinMain(HINSTANCE hInstance,
                                 HINSTANCE /*hPrev*/,
                                 LPWSTR /*lpCmdLine*/,
                                 int /*nShow*/)
{
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW) return 1;

    bool hasFlag = false;
    LPCWSTR firstPositional = nullptr;
    for (int i = 1; i < argc; i++) {
        if (argvW[i][0] == L'/' || argvW[i][0] == L'-') {
            hasFlag = true;
        } else if (!firstPositional) {
            firstPositional = argvW[i];
        }
    }

    if (hasFlag) {
        AttachParentConsole();
        std::vector<std::string> argStrings;
        argStrings.reserve(argc);
        std::vector<char*> argvA;
        argvA.reserve(argc + 1);
        for (int i = 0; i < argc; i++) {
            argStrings.push_back(WideToAcp(argvW[i]));
            argvA.push_back(argStrings.back().data());
        }
        argvA.push_back(nullptr);
        const int rc = cli_main(argc, argvA.data());
        LocalFree(argvW);
        return rc;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    int rc = RunInjectGui(hInstance, firstPositional);
    LocalFree(argvW);
    return rc;
}

// End of File
