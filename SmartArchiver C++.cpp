#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

#pragma execution_character_set("utf-8")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// ---------------------------------------------------------------------------
// 리소스 ID 번호 명시적 고정 (130번/131번 순서 정정 반영)
// ---------------------------------------------------------------------------
const int RES_ID_NANA = 131; // nana.ico 리소스 ID
const int RES_ID_7Z = 130;   // 7z.ico 리소스 ID

// ---------------------------------------------------------------------------
// 압축 엔진 구조체
// ---------------------------------------------------------------------------
struct ArchiverEngine {
    std::wstring name;          // 예: L"NanaZip", L"7-Zip"
    std::wstring guiPath;       // GUI 대화상자 실행 경로
    std::wstring consolePath;   // 콘솔 분석기 실행 경로
    std::wstring iconPath;      // 레지스트리 등록용 아이콘 소스 (예: "C:\path\AN7.exe,-130")
    HICON hIcon = NULL;         // UI 표시용 32x32 아이콘 핸들
    bool isRegistered = false;  // 현재 레지스트리 등록 여부
    bool isChecked = false;     // GUI 설정창 체크 상태
    HWND hCheckbox = NULL;      // 체크박스 윈도우 핸들
};

std::vector<ArchiverEngine> g_Engines;

// 새 폴더 옵션 전역 플래그
bool g_EnableNewFolderMenu = true;
bool g_FolderUseInputBox = true;
bool g_FolderUseDate = false;

std::map<std::wstring, std::wstring> g_LangMap;

// ---------------------------------------------------------------------------
// 순수 Win32 기반 100% 안 깨지는 UTF-8 정밀 로거
// ---------------------------------------------------------------------------
std::wstring GetSelfExePath()
{
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    return std::wstring(selfPath);
}

std::wstring GetDirectoryName(const std::wstring& path)
{
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    return path.substr(0, pos);
}

std::wstring GetAppDir()
{
    return GetDirectoryName(GetSelfExePath());
}

void LogDebug(const std::wstring& message)
{
    // [로그 비활성화] 문제 해결 완료로 인한 로그 파일 기록 주석 처리
    /*
    std::wstring logPath = GetAppDir() + L"\\AN7_Debug.log";
    HANDLE hFile = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t timeBuf[64];
        swprintf_s(timeBuf, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        std::wstring fullLine = std::wstring(timeBuf) + message + L"\r\n";

        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, fullLine.c_str(), (int)fullLine.length(), NULL, 0, NULL, NULL);
        if (utf8Size > 0)
        {
            std::vector<char> utf8Buf(utf8Size);
            WideCharToMultiByte(CP_UTF8, 0, fullLine.c_str(), (int)fullLine.length(), utf8Buf.data(), utf8Size, NULL, NULL);
            DWORD written = 0;
            WriteFile(hFile, utf8Buf.data(), (DWORD)utf8Size, &written, NULL);
        }
        CloseHandle(hFile);
    }
    */
}

// ---------------------------------------------------------------------------
// 보조 함수
// ---------------------------------------------------------------------------
bool FileExists(const std::wstring& path)
{
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DirectoryExists(const std::wstring& path)
{
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring GetFileNameWithoutExtension(const std::wstring& path)
{
    size_t lastSlash = path.find_last_of(L"\\/");
    std::wstring filename = (lastSlash == std::wstring::npos) ? path : path.substr(lastSlash + 1);
    size_t lastDot = filename.find_last_of(L'.');
    if (lastDot == std::wstring::npos) return filename;
    return filename.substr(0, lastDot);
}

// ---------------------------------------------------------------------------
// 명시적 리소스 번호 직결 로더 (정확한 ID 번호로 100% 직접 로드)
// ---------------------------------------------------------------------------
HICON LoadSpecificResourceIcon(int specificResId, const std::wstring& engineName, std::wstring& outIconPath)
{
    std::wstring selfExe = GetSelfExePath();
    HMODULE hMod = GetModuleHandleW(NULL);

    // [로그 비활성화] LogDebug(L"[" + engineName + L"] 리소스 ID " + std::to_wstring(specificResId) + L"번 직접 직결 로드 시도");

    // 레지스트리 형식: "C:\경로\AN7.exe",-130 또는 "C:\경로\AN7.exe",-131
    outIconPath = L"\"" + selfExe + L"\",-" + std::to_wstring(specificResId);

    // 지정된 ID 번호로 HICON 직접 추출
    HICON hIcon = (HICON)LoadImageW(hMod, MAKEINTRESOURCEW(specificResId), IMAGE_ICON, 32, 32, LR_SHARED);
    if (!hIcon)
    {
        hIcon = (HICON)LoadImageW(hMod, MAKEINTRESOURCEW(specificResId), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    }

    if (hIcon)
    {
        // [로그 비활성화] LogDebug(L"  -> [성공] 리소스 ID " + std::to_wstring(specificResId) + L"번 LoadImageW 완벽 성공! HICON 핸들=" + std::to_wstring((UINT_PTR)hIcon));
        return hIcon;
    }
    else
    {
        // [로그 비활성화] LogDebug(L"  -> [실패] 리소스 ID " + std::to_wstring(specificResId) + L"번 로드 실패! GetLastError=" + std::to_wstring(GetLastError()));
    }

    return NULL;
}

// ---------------------------------------------------------------------------
// 외부 lang.json 로드 함수 (자동 생성 일체 안 함 / 파일이 있을 때만 읽음)
// ---------------------------------------------------------------------------
void LoadExternalLangJsonIfExists()
{
    std::wstring jsonPath = GetAppDir() + L"\\lang.json";

    if (!FileExists(jsonPath)) return;

    HANDLE hFile = CreateFileW(jsonPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);
        return;
    }

    std::vector<char> buffer(fileSize + 1, 0);
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, NULL, 0);
    if (wideSize <= 0) return;
    std::wstring content(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, &content[0], wideSize);

    std::wstringstream ss(content);
    std::wstring line;
    while (std::getline(ss, line))
    {
        size_t colonPos = line.find(L':');
        if (colonPos == std::wstring::npos) continue;

        size_t keyStart = line.find(L'\"');
        if (keyStart == std::wstring::npos || keyStart >= colonPos) continue;
        size_t keyEnd = line.find(L'\"', keyStart + 1);
        if (keyEnd == std::wstring::npos || keyEnd >= colonPos) continue;

        std::wstring key = line.substr(keyStart + 1, keyEnd - keyStart - 1);

        size_t valStart = line.find(L'\"', colonPos);
        if (valStart == std::wstring::npos) continue;
        size_t valEnd = line.rfind(L'\"');
        if (valEnd == std::wstring::npos || valEnd <= valStart) continue;

        std::wstring val = line.substr(valStart + 1, valEnd - valStart - 1);

        std::wstring unescaped;
        for (size_t i = 0; i < val.length(); ++i)
        {
            if (val[i] == L'\\' && i + 1 < val.length())
            {
                if (val[i + 1] == L'\"') { unescaped += L'\"'; i++; continue; }
                if (val[i + 1] == L'\\') { unescaped += L'\\'; i++; continue; }
                if (val[i + 1] == L'n') { unescaped += L'\n'; i++; continue; }
                if (val[i + 1] == L't') { unescaped += L'\t'; i++; continue; }
            }
            unescaped += val[i];
        }

        g_LangMap[key] = unescaped;
    }
}

std::wstring L(const std::wstring& key, const std::wstring& defaultKoreanVal)
{
    auto it = g_LangMap.find(key);
    if (it != g_LangMap.end() && !it->second.empty())
    {
        return it->second;
    }
    return defaultKoreanVal;
}

// ---------------------------------------------------------------------------
// Windows 시스템 다크 모드 감지 (AppsUseLightTheme)
// ---------------------------------------------------------------------------
bool IsSystemDarkMode()
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD lightTheme = 1;
        DWORD size = sizeof(lightTheme);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&lightTheme, &size) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return (lightTheme == 0);
        }
        RegCloseKey(hKey);
    }
    return false;
}

// ---------------------------------------------------------------------------
// 레지스트리 App Paths 조회
// ---------------------------------------------------------------------------
std::wstring FindFromAppPaths(const std::wstring& exeName)
{
    const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (HKEY hRoot : roots)
    {
        std::wstring subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName;
        HKEY hKey;
        if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            wchar_t path[MAX_PATH];
            DWORD size = sizeof(path);
            DWORD type = 0;
            if (RegQueryValueExW(hKey, NULL, NULL, &type, (LPBYTE)path, &size) == ERROR_SUCCESS)
            {
                RegCloseKey(hKey);
                if (FileExists(path)) return path;
            }
            RegCloseKey(hKey);
        }
    }
    return L"";
}

// ---------------------------------------------------------------------------
// 압축 엔진 스캔 (NanaZip=131번 직결, 7-Zip=130번 직결로 순서 교정)
// ---------------------------------------------------------------------------
void ScanArchiverEngines()
{
    g_Engines.clear();

    // 1. NanaZip 탐색 -> RES_ID_NANA(131번) 직접 결속
    std::wstring nanaGui = FindFromAppPaths(L"K7G.exe");
    if (nanaGui.empty()) nanaGui = FindFromAppPaths(L"NanaZipG.exe");
    if (nanaGui.empty()) nanaGui = FindFromAppPaths(L"NanaZip.exe");

    std::wstring nanaConsole = FindFromAppPaths(L"K7C.exe");
    if (nanaConsole.empty()) nanaConsole = FindFromAppPaths(L"NanaZipC.exe");
    if (nanaConsole.empty()) nanaConsole = FindFromAppPaths(L"K7.exe");

    if (nanaGui.empty() || nanaConsole.empty())
    {
        wchar_t pathEnv[32767];
        if (GetEnvironmentVariableW(L"PATH", pathEnv, 32767) > 0)
        {
            std::wstringstream ss(pathEnv);
            std::wstring item;
            while (std::getline(ss, item, L';'))
            {
                if (item.empty()) continue;
                if (nanaGui.empty())
                {
                    if (FileExists(item + L"\\K7G.exe")) nanaGui = item + L"\\K7G.exe";
                    else if (FileExists(item + L"\\NanaZip.exe")) nanaGui = item + L"\\NanaZip.exe";
                }
                if (nanaConsole.empty())
                {
                    if (FileExists(item + L"\\K7C.exe")) nanaConsole = item + L"\\K7C.exe";
                    else if (FileExists(item + L"\\NanaZipC.exe")) nanaConsole = item + L"\\NanaZipC.exe";
                }
            }
        }
    }

    if (!nanaGui.empty() && !nanaConsole.empty())
    {
        ArchiverEngine eng;
        eng.name = L"NanaZip";
        eng.guiPath = nanaGui;
        eng.consolePath = nanaConsole;
        // 131번 리소스 직접 매핑
        eng.hIcon = LoadSpecificResourceIcon(RES_ID_NANA, L"NanaZip", eng.iconPath);
        g_Engines.push_back(eng);
    }

    // 2. 7-Zip 탐색 -> RES_ID_7Z(130번) 직접 결속
    const std::vector<std::pair<std::wstring, std::wstring>> classicDirs = {
        { L"7-Zip", L"C:\\Program Files\\7-Zip" },
        { L"7-Zip-Zstandard", L"C:\\Program Files\\7-Zip-Zstandard" },
        { L"7-Zip (x86)", L"C:\\Program Files (x86)\\7-Zip" }
    };

    for (const auto& item : classicDirs)
    {
        std::wstring gui = item.second + L"\\7zG.exe";
        if (!FileExists(gui)) gui = item.second + L"\\7z-zstd-g.exe";

        std::wstring console = item.second + L"\\7z.exe";
        if (!FileExists(console)) console = item.second + L"\\7z-zstd.exe";

        if (FileExists(gui) && FileExists(console))
        {
            ArchiverEngine eng;
            eng.name = item.first;
            eng.guiPath = gui;
            eng.consolePath = console;
            // 130번 리소스 직접 매핑
            eng.hIcon = LoadSpecificResourceIcon(RES_ID_7Z, eng.name, eng.iconPath);
            g_Engines.push_back(eng);
        }
    }
}

// ---------------------------------------------------------------------------
// 레지스트리 경로 정의 및 상태 점검
// ---------------------------------------------------------------------------
const wchar_t* REG_BASE_EXTRACT = L"Software\\Classes\\*\\shell";
const wchar_t* REG_FOLDER_BG = L"Software\\Classes\\Directory\\Background\\shell\\SmartNewFolder";

void CheckRegistrationStatus()
{
    for (auto& eng : g_Engines)
    {
        std::wstring keyName = std::wstring(REG_BASE_EXTRACT) + L"\\SmartExtract_" + eng.name;
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyName.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            eng.isRegistered = true;
            eng.isChecked = true;
            RegCloseKey(hKey);
        }
        else
        {
            eng.isRegistered = false;
            eng.isChecked = true;
        }
    }

    HKEY hCmdKey;
    std::wstring cmdKeyPath = std::wstring(REG_FOLDER_BG) + L"\\command";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, cmdKeyPath.c_str(), 0, KEY_READ, &hCmdKey) == ERROR_SUCCESS)
    {
        g_EnableNewFolderMenu = true;
        wchar_t cmdBuf[512];
        DWORD size = sizeof(cmdBuf);
        if (RegQueryValueExW(hCmdKey, NULL, NULL, NULL, (LPBYTE)cmdBuf, &size) == ERROR_SUCCESS)
        {
            g_FolderUseInputBox = (wcsstr(cmdBuf, L"input") != NULL);
            g_FolderUseDate = (wcsstr(cmdBuf, L"date") != NULL);
        }
        RegCloseKey(hCmdKey);
    }
    else
    {
        g_EnableNewFolderMenu = true;
    }
}

// ---------------------------------------------------------------------------
// 레지스트리 적용/해제
// ---------------------------------------------------------------------------
void ApplyContextMenuRegistration(HWND hWnd)
{
    std::wstring exePath = GetSelfExePath();

    HKEY hShell;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_BASE_EXTRACT, 0, KEY_ALL_ACCESS, &hShell) == ERROR_SUCCESS)
    {
        wchar_t subKeyName[256];
        DWORD index = 0, size = 256;
        std::vector<std::wstring> toDelete;
        while (RegEnumKeyExW(hShell, index++, subKeyName, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            if (wcsstr(subKeyName, L"SmartExtract") != NULL) toDelete.push_back(subKeyName);
            size = 256;
        }
        for (const auto& k : toDelete) RegDeleteTreeW(hShell, k.c_str());
        RegCloseKey(hShell);
    }

    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shell\\SmartNewFolderTime");
    RegDeleteTreeW(HKEY_CURRENT_USER, REG_FOLDER_BG);

    std::vector<ArchiverEngine> selected;
    for (const auto& eng : g_Engines)
    {
        if (eng.isChecked) selected.push_back(eng);
    }

    if (selected.empty() && !g_EnableNewFolderMenu)
    {
        std::wstring msgUnreg = L(L"msg_unregistered", L"모든 우클릭 메뉴 등록이 해제되었습니다.");
        MessageBoxW(hWnd, msgUnreg.c_str(), L"AN7", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const wchar_t* appliesTo = L"System.FileExtension:=.zip OR System.FileExtension:=.7z OR System.FileExtension:=.rar OR System.FileExtension:=.tar OR System.FileExtension:=.gz OR System.FileExtension:=.xz OR System.FileExtension:=.bz2";

    std::wstring baseExtractName = L(L"menu_extract", L"알아서 풀기");

    for (size_t i = 0; i < selected.size(); ++i)
    {
        const auto& eng = selected[i];
        std::wstring keyName = std::wstring(REG_BASE_EXTRACT) + L"\\SmartExtract_" + eng.name;
        std::wstring menuName = (selected.size() == 1) ? baseExtractName : baseExtractName + L" (" + eng.name + L")";

        HKEY hKey, hCmdKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyName.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
        {
            const wchar_t* posTop = L"Top";
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)menuName.c_str(), (DWORD)((menuName.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"Position", 0, REG_SZ, (const BYTE*)posTop, (DWORD)((wcslen(posTop) + 1) * sizeof(wchar_t)));

            // [로그 비활성화] LogDebug(L"레지스트리 Icon 등록값 [" + eng.name + L"]: " + eng.iconPath);
            RegSetValueExW(hKey, L"Icon", 0, REG_SZ, (const BYTE*)eng.iconPath.c_str(), (DWORD)((eng.iconPath.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"AppliesTo", 0, REG_SZ, (const BYTE*)appliesTo, (DWORD)((wcslen(appliesTo) + 1) * sizeof(wchar_t)));

            if (RegCreateKeyExW(hKey, L"command", 0, NULL, 0, KEY_WRITE, NULL, &hCmdKey, NULL) == ERROR_SUCCESS)
            {
                std::wstring cmd = L"\"" + exePath + L"\" extract_engine \"" + eng.guiPath + L"\" \"" + eng.consolePath + L"\" \"%1\"";
                RegSetValueExW(hCmdKey, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hCmdKey);
            }
            RegCloseKey(hKey);
        }
    }

    if (g_EnableNewFolderMenu)
    {
        HKEY hFKey, hFCmdKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_FOLDER_BG, 0, NULL, 0, KEY_WRITE, NULL, &hFKey, NULL) == ERROR_SUCCESS)
        {
            std::wstring menuName = L(L"menu_new_folder", L"새 폴더");
            const wchar_t* posTop = L"Top";
            const wchar_t* iconVal = g_FolderUseDate ? L"shell32.dll,21" : L"shell32.dll,3";

            RegSetValueExW(hFKey, NULL, 0, REG_SZ, (const BYTE*)menuName.c_str(), (DWORD)((menuName.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hFKey, L"Position", 0, REG_SZ, (const BYTE*)posTop, (DWORD)((wcslen(posTop) + 1) * sizeof(wchar_t)));
            RegSetValueExW(hFKey, L"Icon", 0, REG_SZ, (const BYTE*)iconVal, (DWORD)((wcslen(iconVal) + 1) * sizeof(wchar_t)));

            if (RegCreateKeyExW(hFKey, L"command", 0, NULL, 0, KEY_WRITE, NULL, &hFCmdKey, NULL) == ERROR_SUCCESS)
            {
                std::wstring modeStr = g_FolderUseInputBox ? L"input" : L"direct";
                std::wstring dateStr = g_FolderUseDate ? L"date" : L"normal";
                std::wstring cmd = L"\"" + exePath + L"\" create_folder " + modeStr + L" " + dateStr + L" \"%V\"";
                RegSetValueExW(hFCmdKey, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hFCmdKey);
            }
            RegCloseKey(hFKey);
        }
    }

    std::wstring msgSuccess = L(L"msg_success", L"우클릭 메뉴 등록이 완료되었습니다!");
    MessageBoxW(hWnd, msgSuccess.c_str(), L"AN7", MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// 압축 프로세스 실행 헬퍼
// ---------------------------------------------------------------------------
std::wstring RunArchiverAndReadOutput(const std::wstring& archiverPath, const std::wstring& arguments)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return L"";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmdLine = L"\"" + archiverPath + L"\" " + arguments;
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    if (!CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return L"";
    }
    CloseHandle(hWritePipe);

    std::string rawOutput;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0)
    {
        rawOutput.append(buffer, bytesRead);
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (rawOutput.empty()) return L"";

    int wideSize = MultiByteToWideChar(CP_OEMCP, 0, rawOutput.c_str(), (int)rawOutput.size(), NULL, 0);
    std::wstring wideOutput(wideSize, 0);
    MultiByteToWideChar(CP_OEMCP, 0, rawOutput.c_str(), (int)rawOutput.size(), &wideOutput[0], wideSize);

    return wideOutput;
}

HANDLE RunArchiver(const std::wstring& archiverPath, const std::wstring& arguments, bool showWindow = true)
{
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = archiverPath.c_str();
    sei.lpParameters = arguments.c_str();
    sei.nShow = showWindow ? SW_SHOWNORMAL : SW_HIDE;

    if (ShellExecuteExW(&sei)) return sei.hProcess;

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOWNORMAL : SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmdLine = L"\"" + archiverPath + L"\" " + arguments;
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    if (!CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return NULL;
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// ---------------------------------------------------------------------------
// "알아서 풀기" 분석 로직
// ---------------------------------------------------------------------------
void AddArchiveRootItem(std::map<std::wstring, bool>& rootItems, std::wstring currentPath, const std::wstring& attributes, bool folderFlag)
{
    while (!currentPath.empty() && (currentPath.front() == L'\\' || currentPath.front() == L'/' || currentPath.front() == L' '))
        currentPath.erase(currentPath.begin());

    if (currentPath.empty()) return;

    size_t backslashIndex = currentPath.find(L'\\');
    size_t slashIndex = currentPath.find(L'/');
    size_t separatorIndex = std::wstring::npos;

    if (backslashIndex == std::wstring::npos) separatorIndex = slashIndex;
    else if (slashIndex == std::wstring::npos) separatorIndex = backslashIndex;
    else separatorIndex = (std::min)(backslashIndex, slashIndex);

    std::wstring rootItemName = (separatorIndex != std::wstring::npos) ? currentPath.substr(0, separatorIndex) : currentPath;
    if (rootItemName.empty()) return;

    bool isDirectory = (separatorIndex != std::wstring::npos) || folderFlag || (attributes.find(L"D") != std::wstring::npos);

    if (rootItems.find(rootItemName) == rootItems.end()) rootItems[rootItemName] = isDirectory;
    else if (isDirectory) rootItems[rootItemName] = true;
}

HANDLE SmartExtract(const std::wstring& archivePath, const std::wstring& guiPath, const std::wstring& consolePath)
{
    if (!FileExists(archivePath)) return NULL;

    std::wstring arguments = L"l -slt \"" + archivePath + L"\"";
    std::wstring output = RunArchiverAndReadOutput(consolePath, arguments);

    std::map<std::wstring, bool> rootItems;
    std::wstringstream ss(output);
    std::wstring line;

    bool isListingSection = false;
    std::wstring currentPath = L"";
    std::wstring currentAttributes = L"";
    bool currentFolder = false;

    while (std::getline(ss, line))
    {
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        if (line.rfind(L"----------", 0) == 0)
        {
            isListingSection = true;
            continue;
        }
        if (!isListingSection) continue;

        if (line.rfind(L"Path = ", 0) == 0)
        {
            if (!currentPath.empty()) AddArchiveRootItem(rootItems, currentPath, currentAttributes, currentFolder);
            currentPath = line.substr(7);
            currentAttributes = L"";
            currentFolder = false;
        }
        else if (line.rfind(L"Attributes = ", 0) == 0)
        {
            currentAttributes = line.substr(13);
        }
        else if (line.rfind(L"Folder = ", 0) == 0)
        {
            currentFolder = (line.substr(9) == L"+");
        }
    }

    if (!currentPath.empty()) AddArchiveRootItem(rootItems, currentPath, currentAttributes, currentFolder);

    std::wstring outputDir = GetDirectoryName(archivePath);
    bool createFolder = !(rootItems.size() == 1 && rootItems.begin()->second == true);

    std::wstring finalOutputDir = outputDir;
    if (createFolder)
    {
        finalOutputDir = outputDir + L"\\" + GetFileNameWithoutExtension(archivePath);
        CreateDirectoryW(finalOutputDir.c_str(), NULL);
    }

    std::wstring extractArgs = L"x \"" + archivePath + L"\" -o\"" + finalOutputDir + L"\"";
    return RunArchiver(guiPath, extractArgs, true);
}

// ---------------------------------------------------------------------------
// 중복 방지 폴더 이름 생성기
// ---------------------------------------------------------------------------
std::wstring GenerateUniqueFolderPath(const std::wstring& parentDir, const std::wstring& baseName)
{
    std::wstring path = parentDir + L"\\" + baseName;
    if (DirectoryExists(path) || FileExists(path))
    {
        int count = 1;
        while (true)
        {
            path = parentDir + L"\\" + baseName + L" (" + std::to_wstring(count) + L")";
            if (!DirectoryExists(path) && !FileExists(path)) break;
            count++;
        }
    }
    return path;
}

// ---------------------------------------------------------------------------
// 시스템 다크/라이트 모드 연동 풀 렌더링 인풋 박스
// ---------------------------------------------------------------------------
struct InputBoxParams {
    std::wstring parentDir;
    std::wstring defaultName;
    bool confirmed = false;
    std::wstring resultName;
};

WNDPROC g_OrigEditProc = NULL;

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN)
    {
        if (wParam == VK_RETURN)
        {
            PostMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDOK, 0), (LPARAM)hWnd);
            return 0;
        }
        else if (wParam == VK_ESCAPE)
        {
            PostMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), (LPARAM)hWnd);
            return 0;
        }
    }
    return CallWindowProc(g_OrigEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK InputBoxWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static InputBoxParams* pParams = NULL;
    static HWND hEdit = NULL;
    static HFONT hFont = NULL;
    static HFONT hFontSmall = NULL;
    static HBRUSH hBgBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static bool isDark = false;

    switch (message)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
        pParams = (InputBoxParams*)pcs->lpCreateParams;
        isDark = IsSystemDarkMode();

        if (isDark)
        {
            BOOL darkVal = TRUE;
            DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkVal, sizeof(darkVal));
            hBgBrush = CreateSolidBrush(RGB(32, 32, 32));
            hEditBrush = CreateSolidBrush(RGB(45, 45, 45));
        }
        else
        {
            hBgBrush = CreateSolidBrush(RGB(245, 245, 245));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
        }

        hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");

        hFontSmall = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");

        std::wstring promptText = L(L"input_prompt", L"새 폴더 이름 입력:");
        HWND hLabel = CreateWindowExW(0, L"STATIC", promptText.c_str(),
            WS_CHILD | WS_VISIBLE, 18, 12, 320, 20, hWnd, NULL, NULL, NULL);
        SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        hEdit = CreateWindowExW(0, L"EDIT", pParams->defaultName.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 18, 36, 324, 26, hWnd, (HMENU)101, NULL, NULL);
        SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring hintText = L(L"input_hint", L"Enter: 폴더 생성   |   Esc: 취소");
        HWND hHint = CreateWindowExW(0, L"STATIC", hintText.c_str(),
            WS_CHILD | WS_VISIBLE, 18, 68, 320, 18, hWnd, NULL, NULL, NULL);
        SendMessage(hHint, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

        g_OrigEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

        SendMessage(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);

        break;
    }
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, hBgBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        if (isDark)
        {
            SetTextColor(hdc, RGB(225, 225, 225));
            return (LRESULT)hBgBrush;
        }
        else
        {
            SetTextColor(hdc, RGB(40, 40, 40));
            return (LRESULT)hBgBrush;
        }
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        if (isDark)
        {
            SetBkColor(hdc, RGB(45, 45, 45));
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)hEditBrush;
        }
        else
        {
            SetBkColor(hdc, RGB(255, 255, 255));
            SetTextColor(hdc, RGB(0, 0, 0));
            return (LRESULT)hEditBrush;
        }
    }
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK)
        {
            wchar_t buf[MAX_PATH];
            GetWindowTextW(hEdit, buf, MAX_PATH);
            pParams->resultName = buf;
            pParams->confirmed = true;
            DestroyWindow(hWnd);
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            pParams->confirmed = false;
            DestroyWindow(hWnd);
        }
        break;
    }
    case WM_DESTROY:
    {
        if (hFont) DeleteObject(hFont);
        if (hFontSmall) DeleteObject(hFontSmall);
        if (hBgBrush) DeleteObject(hBgBrush);
        if (hEditBrush) DeleteObject(hEditBrush);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 새 폴더 실행 함수
// ---------------------------------------------------------------------------
void ExecuteCreateFolder(const std::wstring& parentDir, bool useInputBox, bool useDate)
{
    if (!DirectoryExists(parentDir)) return;

    std::wstring baseName;
    if (useDate)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[128];
        std::wstring dateFmt = L(L"folder_date_format", L"새 폴더 (%04d-%02d-%02d)");
        swprintf_s(buf, dateFmt.c_str(), st.wYear, st.wMonth, st.wDay);
        baseName = buf;
    }
    else
    {
        baseName = L(L"folder_default_name", L"새 폴더");
    }

    std::wstring uniquePath = GenerateUniqueFolderPath(parentDir, baseName);
    std::wstring defaultFolderName = uniquePath.substr(uniquePath.find_last_of(L"\\/") + 1);

    if (useInputBox)
    {
        InputBoxParams params;
        params.parentDir = parentDir;
        params.defaultName = defaultFolderName;

        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = InputBoxWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"SmartArchiverInputBox";
        RegisterClassExW(&wc);

        int boxW = 370;
        int boxH = 135;

        POINT ptCursor;
        GetCursorPos(&ptCursor);
        HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };

        int posX = (GetSystemMetrics(SM_CXSCREEN) - boxW) / 2;
        int posY = (GetSystemMetrics(SM_CYSCREEN) - boxH) / 2;

        if (GetMonitorInfoW(hMon, &mi))
        {
            posX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - boxW) / 2;
            posY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - boxH) / 2;
        }

        std::wstring winTitle = L(L"input_title", L"새 폴더");
        HWND hWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"SmartArchiverInputBox", winTitle.c_str(),
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, posX, posY, boxW, boxH, NULL, NULL, wc.hInstance, &params);

        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (params.confirmed && !params.resultName.empty())
        {
            std::wstring finalPath = parentDir + L"\\" + params.resultName;
            CreateDirectoryW(finalPath.c_str(), NULL);
            SHChangeNotify(SHCNE_MKDIR, SHCNF_PATHW, finalPath.c_str(), NULL);
            SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, parentDir.c_str(), NULL);
        }
    }
    else
    {
        CreateDirectoryW(uniquePath.c_str(), NULL);
        SHChangeNotify(SHCNE_MKDIR, SHCNF_PATHW, uniquePath.c_str(), NULL);
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, parentDir.c_str(), NULL);
    }
}

// ---------------------------------------------------------------------------
// 순수 Win32 네이티브 미니 설정 창
// ---------------------------------------------------------------------------
#define IDC_BTN_APPLY      1001
#define IDC_BTN_CLOSE      1002
#define IDC_BTN_UNALL      1003
#define IDC_CHK_BASE       2000
#define IDC_CHK_NEWFOLDER  3000
#define IDC_RADIO_MODE_INP 3001
#define IDC_RADIO_MODE_DIR 3002
#define IDC_RADIO_NAME_NRM 3003
#define IDC_RADIO_NAME_DAT 3004

HWND g_hChkFolder = NULL;
HWND g_hRadioModeInp = NULL;
HWND g_hRadioModeDir = NULL;
HWND g_hRadioNameNrm = NULL;
HWND g_hRadioNameDat = NULL;

LRESULT CALLBACK MiniWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HFONT hFont = NULL;

    switch (message)
    {
    case WM_CREATE:
    {
        hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");

        int startY = 15;

        // 1. 압축 엔진 섹션
        std::wstring secArchiver = L(L"section_archiver", L"[ 압축 해제 엔진 선택 (7-Zip 계열) ]");
        HWND hTitle = CreateWindowExW(0, L"STATIC", secArchiver.c_str(),
            WS_CHILD | WS_VISIBLE, 20, startY, 530, 20, hWnd, NULL, NULL, NULL);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

        startY += 26;

        std::wstring activeStr = L(L"status_active", L" [현재 등록됨]");

        for (size_t i = 0; i < g_Engines.size(); ++i)
        {
            auto& eng = g_Engines[i];

            HWND hIconStatic = CreateWindowExW(0, L"STATIC", NULL,
                WS_CHILD | WS_VISIBLE | SS_ICON, 25, startY, 32, 32, hWnd, NULL, NULL, NULL);
            if (eng.hIcon)
            {
                SendMessage(hIconStatic, STM_SETICON, (WPARAM)eng.hIcon, 0);
            }

            std::wstring chkText = eng.name + (eng.isRegistered ? activeStr : L"");
            eng.hCheckbox = CreateWindowExW(0, L"BUTTON", chkText.c_str(),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 68, startY + 5, 450, 24, hWnd, (HMENU)(IDC_CHK_BASE + i), NULL, NULL);
            SendMessage(eng.hCheckbox, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(eng.hCheckbox, BM_SETCHECK, eng.isChecked ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hPathText = CreateWindowExW(0, L"STATIC", eng.guiPath.c_str(),
                WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, 68, startY + 28, 470, 16, hWnd, NULL, NULL, NULL);
            SendMessage(hPathText, WM_SETFONT, (WPARAM)hFont, TRUE);

            startY += 55;
        }

        if (g_Engines.empty())
        {
            HWND hWarn = CreateWindowExW(0, L"STATIC", L"시스템에 지원되는 압축 엔진(NanaZip, 7-Zip)이 설치되어 있지 않습니다.",
                WS_CHILD | WS_VISIBLE, 25, startY, 480, 20, hWnd, NULL, NULL, NULL);
            SendMessage(hWarn, WM_SETFONT, (WPARAM)hFont, TRUE);
            startY += 40;
        }

        startY += 10;

        // 2. 새 폴더 옵션 섹션
        std::wstring secFolder = L(L"section_folder", L"[ 새 폴더 ] 우클릭 배경 메뉴 설정");
        HWND hGrpFolder = CreateWindowExW(0, L"BUTTON", secFolder.c_str(),
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, startY, 530, 150, hWnd, NULL, NULL, NULL);
        SendMessage(hGrpFolder, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring chkFolderTxt = L(L"chk_folder", L"우클릭 배경 메뉴에 '새 폴더' 등록하기");
        g_hChkFolder = CreateWindowExW(0, L"BUTTON", chkFolderTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 35, startY + 22, 490, 22, hWnd, (HMENU)IDC_CHK_NEWFOLDER, NULL, NULL);
        SendMessage(g_hChkFolder, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hChkFolder, BM_SETCHECK, g_EnableNewFolderMenu ? BST_CHECKED : BST_UNCHECKED, 0);

        std::wstring modeLblTxt = L(L"lbl_mode", L"생성 방식:");
        HWND hModeLbl = CreateWindowExW(0, L"STATIC", modeLblTxt.c_str(), WS_CHILD | WS_VISIBLE, 40, startY + 52, 70, 20, hWnd, NULL, NULL, NULL);
        SendMessage(hModeLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring modeInpTxt = L(L"mode_input", L"이름 변경 팝업창 띄우기 (화면 중앙 / 시스템 테마 연동)");
        g_hRadioModeInp = CreateWindowExW(0, L"BUTTON", modeInpTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 115, startY + 52, 420, 20, hWnd, (HMENU)IDC_RADIO_MODE_INP, NULL, NULL);
        SendMessage(g_hRadioModeInp, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring modeDirTxt = L(L"mode_direct", L"팝업 없이 그냥 즉시 생성하기");
        g_hRadioModeDir = CreateWindowExW(0, L"BUTTON", modeDirTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 115, startY + 74, 420, 20, hWnd, (HMENU)IDC_RADIO_MODE_DIR, NULL, NULL);
        SendMessage(g_hRadioModeDir, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring nameLblTxt = L(L"lbl_name", L"기본 이름:");
        HWND hNameLbl = CreateWindowExW(0, L"STATIC", nameLblTxt.c_str(), WS_CHILD | WS_VISIBLE, 40, startY + 102, 70, 20, hWnd, NULL, NULL, NULL);
        SendMessage(hNameLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring nameNrmTxt = L(L"name_normal", L"기본 (\"새 폴더\")");
        g_hRadioNameNrm = CreateWindowExW(0, L"BUTTON", nameNrmTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 115, startY + 102, 180, 20, hWnd, (HMENU)IDC_RADIO_NAME_NRM, NULL, NULL);
        SendMessage(g_hRadioNameNrm, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring nameDatTxt = L(L"name_date", L"날짜 포함 (\"새 폴더 (YYYY-MM-DD)\")");
        g_hRadioNameDat = CreateWindowExW(0, L"BUTTON", nameDatTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 305, startY + 102, 230, 20, hWnd, (HMENU)IDC_RADIO_NAME_DAT, NULL, NULL);
        SendMessage(g_hRadioNameDat, WM_SETFONT, (WPARAM)hFont, TRUE);

        SendMessage(g_hRadioModeInp, BM_SETCHECK, g_FolderUseInputBox ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_hRadioModeDir, BM_SETCHECK, (!g_FolderUseInputBox) ? BST_CHECKED : BST_UNCHECKED, 0);

        SendMessage(g_hRadioNameNrm, BM_SETCHECK, (!g_FolderUseDate) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_hRadioNameDat, BM_SETCHECK, g_FolderUseDate ? BST_CHECKED : BST_UNCHECKED, 0);

        startY += 165;

        // 하단 버튼들
        std::wstring btnApplyTxt = L(L"btn_apply", L"우클릭 메뉴 적용");
        HWND hBtnApply = CreateWindowExW(0, L"BUTTON", btnApplyTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 80, startY, 130, 32, hWnd, (HMENU)IDC_BTN_APPLY, NULL, NULL);
        SendMessage(hBtnApply, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring btnUnregTxt = L(L"btn_unregister", L"전체 해제");
        HWND hBtnUnAll = CreateWindowExW(0, L"BUTTON", btnUnregTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 230, startY, 110, 32, hWnd, (HMENU)IDC_BTN_UNALL, NULL, NULL);
        SendMessage(hBtnUnAll, WM_SETFONT, (WPARAM)hFont, TRUE);

        std::wstring btnCloseTxt = L(L"btn_close", L"닫기");
        HWND hBtnClose = CreateWindowExW(0, L"BUTTON", btnCloseTxt.c_str(),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 360, startY, 110, 32, hWnd, (HMENU)IDC_BTN_CLOSE, NULL, NULL);
        SendMessage(hBtnClose, WM_SETFONT, (WPARAM)hFont, TRUE);

        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        if (wmId >= IDC_CHK_BASE && wmId < IDC_CHK_BASE + (int)g_Engines.size())
        {
            int idx = wmId - IDC_CHK_BASE;
            g_Engines[idx].isChecked = (SendMessage(g_Engines[idx].hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        else if (wmId == IDC_BTN_APPLY)
        {
            for (size_t i = 0; i < g_Engines.size(); ++i)
            {
                g_Engines[i].isChecked = (SendMessage(g_Engines[i].hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            g_EnableNewFolderMenu = (SendMessage(g_hChkFolder, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_FolderUseInputBox = (SendMessage(g_hRadioModeInp, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_FolderUseDate = (SendMessage(g_hRadioNameDat, BM_GETCHECK, 0, 0) == BST_CHECKED);

            ApplyContextMenuRegistration(hWnd);
            DestroyWindow(hWnd);
        }
        else if (wmId == IDC_BTN_UNALL)
        {
            for (size_t i = 0; i < g_Engines.size(); ++i)
            {
                g_Engines[i].isChecked = false;
                SendMessage(g_Engines[i].hCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
            }
            g_EnableNewFolderMenu = false;
            SendMessage(g_hChkFolder, BM_SETCHECK, BST_UNCHECKED, 0);

            ApplyContextMenuRegistration(hWnd);
            DestroyWindow(hWnd);
        }
        else if (wmId == IDC_BTN_CLOSE)
        {
            DestroyWindow(hWnd);
        }
        break;
    }
    case WM_DESTROY:
    {
        if (hFont) DeleteObject(hFont);
        for (auto& eng : g_Engines)
        {
            if (eng.hIcon) DestroyIcon(eng.hIcon);
        }
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

void ShowMiniSettingsWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = { sizeof(wcex) };
    wcex.lpfnWndProc = MiniWindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"AN7MiniSettings";

    RegisterClassExW(&wcex);

    int winHeight = 310 + (int)g_Engines.size() * 58;
    if (winHeight < 400) winHeight = 400;

    int screenX = (GetSystemMetrics(SM_CXSCREEN) - 590) / 2;
    int screenY = (GetSystemMetrics(SM_CYSCREEN) - winHeight) / 2;

    std::wstring appTitle = L(L"app_title", L"AN7 - 7-Zip & NanaZip Adapter");
    HWND hWnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"AN7MiniSettings",
        appTitle.c_str(), WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        screenX, screenY, 590, winHeight, NULL, NULL, hInstance, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ---------------------------------------------------------------------------
// 진입점 (wWinMain)
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;

    // 1. 외부 lang.json 로드
    LoadExternalLangJsonIfExists();

    // 2. 엔진 스캔 (131번 NanaZip, 130번 7-Zip 교정 리소스 직결)
    ScanArchiverEngines();
    CheckRegistrationStatus();

    // 3. 단독 실행 시 설정 창 팝업
    if (argc < 2)
    {
        ShowMiniSettingsWindow(hInstance);
        LocalFree(argv);
        return 0;
    }

    std::wstring command = argv[1];
    std::transform(command.begin(), command.end(), command.begin(), ::towlower);

    // 4. 엔진별 우클릭 압축 해제
    if (command == L"extract_engine" && argc >= 5)
    {
        std::wstring guiPath = argv[2];
        std::wstring consolePath = argv[3];
        std::wstring archivePath = argv[4];

        HANDLE hProc = SmartExtract(archivePath, guiPath, consolePath);
        if (hProc)
        {
            WaitForSingleObject(hProc, INFINITE);
            CloseHandle(hProc);
        }
    }
    // 5. 기존 커맨드라인 호환
    else if (command == L"extract" && argc >= 3)
    {
        std::wstring guiPath = g_Engines.empty() ? L"" : g_Engines[0].guiPath;
        std::wstring consolePath = g_Engines.empty() ? L"" : g_Engines[0].consolePath;

        if (!guiPath.empty() && !consolePath.empty())
        {
            for (int i = 2; i < argc; ++i)
            {
                HANDLE hProc = SmartExtract(argv[i], guiPath, consolePath);
                if (hProc)
                {
                    WaitForSingleObject(hProc, INFINITE);
                    CloseHandle(hProc);
                }
            }
        }
    }
    // 6. 새 폴더 만들기 (create_folder <input/direct> <date/normal> "%V")
    else if (command == L"create_folder")
    {
        bool useInput = (argc > 2 && _wcsicmp(argv[2], L"input") == 0);
        bool useDate = (argc > 3 && _wcsicmp(argv[3], L"date") == 0);
        std::wstring folderPath = (argc > 4) ? argv[4] : L"";

        if (folderPath.empty() && argc > 2)
        {
            folderPath = argv[2];
            useInput = true;
            useDate = false;
        }

        ExecuteCreateFolder(folderPath, useInput, useDate);
    }

    LocalFree(argv);
    return 0;
}