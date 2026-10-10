// helmetfx_ext.cpp: Arma 3 extension (x64). Build as helmetfx_x64.dll

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <string>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#define HELMETFX_PORT 38517

static const wchar_t* PLUGIN_FILE = L"helmetfx_win64.dll";

static SOCKET      g_sock = INVALID_SOCKET;
static sockaddr_in g_addr;

static void put(char* out, int size, const char* text) {
    if (size <= 0) return;
    std::strncpy(out, text, size - 1); out[size - 1] = 0;
}

static bool ensureSocket() {
    if (g_sock != INVALID_SOCKET) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_sock == INVALID_SOCKET) return false;
    std::memset(&g_addr, 0, sizeof g_addr);
    g_addr.sin_family = AF_INET;
    g_addr.sin_port = htons(HELMETFX_PORT);
    g_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return true;
}

static std::wstring moduleDir() {
    HMODULE h = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&moduleDir), &h)) return L"";
    wchar_t buf[1024];
    DWORD n = GetModuleFileNameW(h, buf, 1024);
    if (n == 0 || n >= 1024) return L"";
    std::wstring p(buf, n);
    size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : p.substr(0, slash);
}

static bool pathExists(const std::wstring& p) {
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool isDirectory(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool fileHash(const std::wstring& path, unsigned long long& hash, unsigned long long& size) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    static unsigned char buf[65536];
    unsigned long long h = 1469598103934665603ULL;
    unsigned long long total = 0;
    DWORD n = 0;
    while (ReadFile(f, buf, sizeof buf, &n, nullptr) && n > 0) {
        for (DWORD i = 0; i < n; ++i) { h ^= buf[i]; h *= 1099511628211ULL; }
        total += n;
    }
    CloseHandle(f);
    hash = h;
    size = total;
    return true;
}

static std::string failure(const char* what, DWORD code) {
    return std::string("error: ") + what + " (" + std::to_string(code) + ")";
}

static std::string installPlugin() {
    static bool checked = false;
    if (checked) return "ok: already checked this session";
    checked = true;

    std::wstring src = moduleDir();
    if (src.empty()) return "error: cannot locate the extension folder";
    src += L"\\";
    src += PLUGIN_FILE;
    if (!pathExists(src)) return "error: plugin dll not found next to the extension";

    wchar_t appdata[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return "skip: APPDATA is not set";

    std::wstring profile = std::wstring(appdata) + L"\\TS3Client";
    if (!isDirectory(profile)) return "skip: TeamSpeak profile folder not found (start TeamSpeak once first)";

    std::wstring dir = profile + L"\\plugins";
    if (!isDirectory(dir) && !CreateDirectoryW(dir.c_str(), nullptr)) return failure("cannot create plugins folder", GetLastError());

    std::wstring dst = dir + L"\\" + PLUGIN_FILE;
    std::wstring old = dst + L".old";
    std::wstring tmp = dst + L".tmp";
    DeleteFileW(old.c_str());

    bool hadPlugin = pathExists(dst);
    if (hadPlugin) {
        unsigned long long hs = 0, ss = 0, hd = 0, sd = 0;
        if (fileHash(src, hs, ss) && fileHash(dst, hd, sd) && hs == hd && ss == sd) return "ok: plugin up to date";
    }

    if (!CopyFileW(src.c_str(), tmp.c_str(), FALSE)) return failure("copy failed", GetLastError());

    if (MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING))
        return hadPlugin ? "updated: restart TeamSpeak to load the new plugin"
                         : "installed: restart TeamSpeak to load the plugin";

    if (hadPlugin && MoveFileExW(dst.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        if (MoveFileExW(tmp.c_str(), dst.c_str(), 0))
            return "updated: restart TeamSpeak to load the new plugin";
        DWORD e = GetLastError();
        MoveFileExW(old.c_str(), dst.c_str(), 0);
        DeleteFileW(tmp.c_str());
        return failure("could not put the new plugin in place", e);
    }

    DWORD e = GetLastError();
    DeleteFileW(tmp.c_str());
    return failure("could not replace the plugin", e);
}

extern "C" {

__declspec(dllexport) void __stdcall RVExtensionVersion(char* output, int outputSize) {
    put(output, outputSize, "HelmetFX 0.2");
}

__declspec(dllexport) void __stdcall RVExtension(char* output, int outputSize, const char* function) {
    if (!function) { put(output, outputSize, "ERR null"); return; }
    if (std::strcmp(function, "install") == 0) {
        put(output, outputSize, installPlugin().c_str());
        return;
    }
    if (!ensureSocket()) { put(output, outputSize, "ERR socket"); return; }
    int len = 0;
    while (len < 2000 && function[len]) ++len;
    sendto(g_sock, function, len, 0, (sockaddr*)&g_addr, sizeof g_addr);
    put(output, outputSize, "ok");
}

}