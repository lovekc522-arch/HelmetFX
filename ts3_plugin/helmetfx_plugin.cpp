// compile into helmetfx_win64.dll
// Listens on 127.0.0.1:38517 for "chain:<string>" / "clear" from the Arma extension, builds a Chain on the net thread, applies it in ts3plugin_onEditCapturedVoiceDataEvent.
// Log: %TEMP%\helmetfx_plugin.log

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#include "teamspeak/public_definitions.h"
#include "teamspeak/public_errors.h"
#include "ts3_functions.h"
#include "plugin.h"

#include "helmetfx_core.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <thread>

#define HELMETFX_PORT 38517
static struct TS3Functions ts3Functions;

static const float kSampleRate = 48000.f;
static std::atomic<bool> g_run{false};
static std::thread g_net;

static void fxlog(const char* fmt, ...) {
    char path[MAX_PATH]; DWORD n = GetTempPathA(MAX_PATH, path);
    if (!n || n > MAX_PATH - 32) return;
    std::strcat(path, "helmetfx_plugin.log");
    FILE* f = std::fopen(path, "a"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

// net thread
static void netLoop() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(HELMETFX_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) {
        fxlog("bind failed on port %d (another TS instance?)", HELMETFX_PORT);
        closesocket(s); WSACleanup(); return;
    }
    DWORD timeoutMs = 500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof timeoutMs);
    fxlog("listening on 127.0.0.1:%d", HELMETFX_PORT);

    using clock = std::chrono::steady_clock;
    std::string current;
    auto last = clock::now();
    char buf[2048];

    while (g_run) {
        int n = recv(s, buf, sizeof buf - 1, 0);
        auto now = clock::now();
        if (n > 0) {
            buf[n] = 0;
            std::string msg(buf);
            last = now;
            if (msg == "clear") {
                if (!current.empty()) { g_holder.publish(nullptr); current.clear(); fxlog("chain cleared"); }
            } else if (msg.rfind("chain:", 0) == 0) {
                std::string c = msg.substr(6);
                if (c != current) {
                    std::vector<std::string> warn;
                    g_holder.publish(buildChain(c, kSampleRate, &warn));
                    current = c;
                    fxlog("chain set: %s", c.c_str());
                    for (auto& w : warn) fxlog("  warning: %s", w.c_str());
                }
            }
        } else {
            if (!current.empty() && now - last > std::chrono::seconds(8)) {
                g_holder.publish(nullptr); current.clear();
                fxlog("no heartbeat, chain cleared");
            }
            g_holder.emptyShelf();
        }
    }
    closesocket(s);
    WSACleanup();
}

// TS3 plugin exports
const char* ts3plugin_name()        { return "HelmetFX"; }
const char* ts3plugin_version()     { return "0.1"; }
int         ts3plugin_apiVersion()  { return 26; }   // SDK 26
const char* ts3plugin_author()      { return "Kacie H."; } // im awesome :)
const char* ts3plugin_description() { return "Per-helmet local voice effect chain driven by Arma 3."; }
void        ts3plugin_setFunctionPointers(const struct TS3Functions funcs) { ts3Functions = funcs; }

int ts3plugin_init() {
    g_run = true;
    g_net = std::thread(netLoop);
    fxlog("plugin init");
    return 0;
}

void ts3plugin_shutdown() {
    g_run = false;
    if (g_net.joinable()) g_net.join();
    g_holder.publish(nullptr);
    g_holder.emptyShelf();
    fxlog("plugin shutdown");
}

// audio thread
void ts3plugin_onEditCapturedVoiceDataEvent(uint64 /*serverConnectionHandlerID*/,
                                            short* samples, int sampleCount,
                                            int channels, int* edited) {
    if (!g_holder.grab()) return;

    if (channels == 1) {
        onMicChunk((int16_t*)samples, sampleCount);
    } else {
        static thread_local std::vector<int16_t> mono;
        mono.resize(sampleCount);
        for (int i = 0; i < sampleCount; ++i) mono[i] = samples[i * channels];
        onMicChunk(mono.data(), sampleCount);
        for (int i = 0; i < sampleCount; ++i)
            for (int c = 0; c < channels; ++c) samples[i * channels + c] = mono[i];
    }
    *edited |= 1;
}
