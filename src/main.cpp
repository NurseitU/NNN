#include "config.hpp"
#include "media_server.hpp"
#include "onvif_service.hpp"
#include "ws_discovery.hpp"

#include <glib.h>
#include <cstdio>
#include <string>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <iphlpapi.h>
  #pragma comment(lib, "ws2_32.lib")
  #pragma comment(lib, "iphlpapi.lib")
#endif

// Определить первый не-loopback IPv4-адрес машины.
static std::string detectLocalIpv4() {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    char host[256] = {0};
    if (gethostname(host, sizeof(host)) != 0) return "127.0.0.1";
    addrinfo hints{}; hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) return "127.0.0.1";
    char ip[INET_ADDRSTRLEN] = {0};
    for (addrinfo* p = res; p; p = p->ai_next) {
        auto* a = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        inet_ntop(AF_INET, &a->sin_addr, ip, sizeof(ip));
        if (std::string(ip) != "127.0.0.1") break;
    }
    freeaddrinfo(res);
    return ip[0] ? ip : "127.0.0.1";
#else
    return "127.0.0.1"; // прототип рассчитан на Windows; на Linux задайте Config.localIp вручную
#endif
}

// Простейший разбор аргументов: --monitor N --fps N --bitrate K --rtsp-port P --onvif-port P
static void parseArgs(int argc, char** argv, Config& c) {
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i], v = argv[i + 1];
        if      (k == "--monitor")    c.monitorIndex = std::stoi(v);
        else if (k == "--fps")        c.fps = std::stoi(v);
        else if (k == "--bitrate")    c.bitrateKbps = std::stoi(v);
        else if (k == "--rtsp-port")  c.rtspPort = (uint16_t)std::stoi(v);
        else if (k == "--onvif-port") c.onvifPort = (uint16_t)std::stoi(v);
        else if (k == "--user")       c.user = v;
        else if (k == "--pass")       c.pass = v;
    }
}

int main(int argc, char** argv) {
    Config cfg;
    parseArgs(argc, argv, cfg);
    cfg.localIp = detectLocalIpv4();

    std::printf("=== s2rtsp: экран -> ONVIF/RTSP камера (прототип) ===\n");
    std::printf("Монитор %d, %d fps, %d кбит/с, IP %s\n",
                cfg.monitorIndex, cfg.fps, cfg.bitrateKbps, cfg.localIp.c_str());

    MediaServer media(cfg);
    if (!media.start()) { std::fprintf(stderr, "Ошибка запуска RTSP\n"); return 1; }

    OnvifService onvif(cfg, media);
    if (!onvif.start()) { std::fprintf(stderr, "Ошибка запуска ONVIF\n"); return 1; }

    WsDiscovery wsd(cfg);
    if (!wsd.start()) { std::fprintf(stderr, "Ошибка запуска WS-Discovery\n"); return 1; }

    std::printf("Готово. RTSP: %s\n", media.streamUri().c_str());
    std::printf("Проверка: откройте RTSP в VLC или найдите камеру в ONVIF Device Manager.\n");
    std::printf("Ctrl+C для выхода.\n");

    // GLib main loop нужен gst-rtsp-server для обработки RTSP-сессий.
    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    wsd.stop();
    onvif.stop();
    g_main_loop_unref(loop);
    return 0;
}
