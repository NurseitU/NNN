#include "ws_discovery.hpp"

#include <string>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socklen_t = int;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define closesocket ::close
#endif

namespace {
constexpr char   kMcastAddr[] = "239.255.255.250";
constexpr uint16_t kMcastPort = 3702;

// Наивно вытащить wsa:MessageID входящего Probe, чтобы вернуть его в RelatesTo.
std::string extractMessageId(const std::string& xml) {
    auto p = xml.find("MessageID");
    if (p == std::string::npos) return "";
    auto gt = xml.find('>', p);
    auto lt = xml.find('<', gt);
    if (gt == std::string::npos || lt == std::string::npos) return "";
    return xml.substr(gt + 1, lt - gt - 1);
}
} // namespace

WsDiscovery::WsDiscovery(const Config& cfg) : cfg_(cfg) {}
WsDiscovery::~WsDiscovery() { stop(); }

std::string WsDiscovery::buildProbeMatch(const std::string& relatesTo) const {
    const std::string xaddr =
        "http://" + cfg_.localIp + ":" + std::to_string(cfg_.onvifPort) + "/onvif/device_service";

    // Минимальный, но валидный ProbeMatches (SOAP 1.2 + WS-Discovery + ONVIF).
    return
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
      " xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
      " xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\""
      " xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">"
      "<s:Header>"
        "<a:MessageID>" + cfg_.uuid + "-pm</a:MessageID>"
        "<a:RelatesTo>" + relatesTo + "</a:RelatesTo>"
        "<a:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:To>"
        "<a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</a:Action>"
      "</s:Header>"
      "<s:Body>"
        "<d:ProbeMatches><d:ProbeMatch>"
          "<a:EndpointReference><a:Address>" + cfg_.uuid + "</a:Address></a:EndpointReference>"
          "<d:Types>dn:NetworkVideoTransmitter</d:Types>"
          "<d:Scopes>"
            "onvif://www.onvif.org/type/video_encoder "
            "onvif://www.onvif.org/Profile/Streaming "
            "onvif://www.onvif.org/name/" + cfg_.deviceName + " "
            "onvif://www.onvif.org/hardware/" + cfg_.model +
          "</d:Scopes>"
          "<d:XAddrs>" + xaddr + "</d:XAddrs>"
          "<d:MetadataVersion>1</d:MetadataVersion>"
        "</d:ProbeMatch></d:ProbeMatches>"
      "</s:Body></s:Envelope>";
}

bool WsDiscovery::start() {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    sock_ = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ < 0) return false;

    int reuse = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(kMcastPort);
    if (bind(sock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(sock_); sock_ = -1; return false;
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(kMcastAddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));

    running_ = true;
    worker_ = std::thread([this] { run(); });
    std::printf("[wsd] WS-Discovery слушает %s:%u\n", kMcastAddr, kMcastPort);
    return true;
}

void WsDiscovery::run() {
    char buf[8192];
    while (running_) {
        sockaddr_in from{}; socklen_t fl = sizeof(from);
        int n = recvfrom(sock_, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fl);
        if (n <= 0) continue;
        buf[n] = '\0';
        std::string xml(buf, n);

        // Отвечаем только на Probe c типом NetworkVideoTransmitter (или на любой Probe).
        if (xml.find("Probe") == std::string::npos) continue;
        if (xml.find("NetworkVideoTransmitter") == std::string::npos &&
            xml.find("Device") == std::string::npos) {
            // Некоторые клиенты шлют Probe без типа — тоже отвечаем.
        }

        const std::string reply = buildProbeMatch(extractMessageId(xml));
        sendto(sock_, reply.c_str(), (int)reply.size(), 0, (sockaddr*)&from, fl);
    }
}

void WsDiscovery::stop() {
    if (!running_) return;
    running_ = false;
    if (sock_ >= 0) { closesocket(sock_); sock_ = -1; }
    if (worker_.joinable()) worker_.join();
}
