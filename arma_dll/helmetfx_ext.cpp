// helmetfx_ext.cpp: Arma 3 extension (x64). Build as helmetfx_x64.dll

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstring>
#include <cstdio>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#define HELMETFX_PORT 38517

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

extern "C" {

__declspec(dllexport) void __stdcall RVExtensionVersion(char* output, int outputSize) {
    put(output, outputSize, "HelmetFX 0.1");
}

__declspec(dllexport) void __stdcall RVExtension(char* output, int outputSize, const char* function) {
    if (!function || !ensureSocket()) { put(output, outputSize, "ERR socket"); return; }
    int len = 0; while (len < 2000 && function[len]) ++len;
    sendto(g_sock, function, len, 0, (sockaddr*)&g_addr, sizeof g_addr);
    put(output, outputSize, "ok");
}

}
