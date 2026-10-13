#include "onvif_service.hpp"

#include <string>
#include <cstring>
#include <cstdio>
#include <ctime>

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

std::string envelope(const std::string& body) {
    return
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
      " xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\""
      " xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\""
      " xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
      "<s:Body>" + body + "</s:Body></s:Envelope>";
}

std::string httpResponse(const std::string& xml) {
    return
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/soap+xml; charset=utf-8\r\n"
      "Content-Length: " + std::to_string(xml.size()) + "\r\n"
      "Connection: close\r\n\r\n" + xml;
}

[[maybe_unused]] std::string utcNow() {
    std::time_t t = std::time(nullptr);
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char b[64];
    std::snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
                  g.tm_hour, g.tm_min, g.tm_sec);
    return b;
}

} // namespace

OnvifService::OnvifService(const Config& cfg, const MediaServer& media)
    : cfg_(cfg), media_(media) {}

OnvifService::~OnvifService() { stop(); }

std::string OnvifService::dispatch(const std::string& body) const {
    const std::string devXAddr =
        "http://" + cfg_.localIp + ":" + std::to_string(cfg_.onvifPort) + "/onvif/device_service";
    const std::string medXAddr =
        "http://" + cfg_.localIp + ":" + std::to_string(cfg_.onvifPort) + "/onvif/media_service";
    const std::string rtsp = media_.streamUri();

    auto has = [&](const char* op) { return body.find(op) != std::string::npos; };

    // --- Device service ---
    if (has("GetSystemDateAndTime")) {
        return envelope(
          "<tds:GetSystemDateAndTimeResponse><tds:SystemDateAndTime>"
          "<tt:DateTimeType>NTP</tt:DateTimeType><tt:DaylightSavings>false</tt:DaylightSavings>"
          "<tt:UTCDateTime><tt:Time><tt:Hour>0</tt:Hour></tt:Time></tt:UTCDateTime>"
          "</tds:SystemDateAndTime></tds:GetSystemDateAndTimeResponse>");
        // Примечание: реальное время можно подставить из utcNow(); многим VMS хватает структуры.
    }
    if (has("GetDeviceInformation")) {
        return envelope(
          "<tds:GetDeviceInformationResponse>"
          "<tds:Manufacturer>" + cfg_.manufacturer + "</tds:Manufacturer>"
          "<tds:Model>" + cfg_.model + "</tds:Model>"
          "<tds:FirmwareVersion>" + cfg_.firmware + "</tds:FirmwareVersion>"
          "<tds:SerialNumber>" + cfg_.serial + "</tds:SerialNumber>"
          "<tds:HardwareId>1.0</tds:HardwareId>"
          "</tds:GetDeviceInformationResponse>");
    }
    if (has("GetServices") || has("GetCapabilities")) {
        return envelope(
          "<tds:GetServicesResponse>"
          "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>"
          "<tds:XAddr>" + devXAddr + "</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service>"
          "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>"
          "<tds:XAddr>" + medXAddr + "</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service>"
          "</tds:GetServicesResponse>");
    }
    if (has("GetScopes")) {
        return envelope(
          "<tds:GetScopesResponse>"
          "<tds:Scopes><tt:ScopeItem>onvif://www.onvif.org/Profile/Streaming</tt:ScopeItem></tds:Scopes>"
          "<tds:Scopes><tt:ScopeItem>onvif://www.onvif.org/name/" + cfg_.deviceName + "</tt:ScopeItem></tds:Scopes>"
          "</tds:GetScopesResponse>");
    }

    // --- Media service ---
    if (has("GetProfiles")) {
        return envelope(
          "<trt:GetProfilesResponse><trt:Profiles token=\"prof0\" fixed=\"true\">"
          "<tt:Name>MainStream</tt:Name>"
          "<tt:VideoSourceConfiguration token=\"vsc0\"><tt:Name>vsc0</tt:Name>"
            "<tt:SourceToken>vs0</tt:SourceToken>"
            "<tt:Bounds x=\"0\" y=\"0\" width=\"1920\" height=\"1080\"/></tt:VideoSourceConfiguration>"
          "<tt:VideoEncoderConfiguration token=\"vec0\"><tt:Name>vec0</tt:Name>"
            "<tt:Encoding>H264</tt:Encoding>"
            "<tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>"
            "<tt:RateControl><tt:FrameRateLimit>" + std::to_string(cfg_.fps) + "</tt:FrameRateLimit>"
            "<tt:BitrateLimit>" + std::to_string(cfg_.bitrateKbps) + "</tt:BitrateLimit></tt:RateControl>"
            "<tt:H264><tt:GovLength>" + std::to_string(cfg_.fps) + "</tt:GovLength>"
            "<tt:H264Profile>High</tt:H264Profile></tt:H264>"
          "</tt:VideoEncoderConfiguration>"
          "</trt:Profiles></trt:GetProfilesResponse>");
    }
    if (has("GetVideoSources")) {
        return envelope(
          "<trt:GetVideoSourcesResponse><trt:VideoSources token=\"vs0\">"
          "<tt:Framerate>" + std::to_string(cfg_.fps) + "</tt:Framerate>"
          "<tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>"
          "</trt:VideoSources></trt:GetVideoSourcesResponse>");
    }
    if (has("GetStreamUri")) {
        return envelope(
          "<trt:GetStreamUriResponse><trt:MediaUri>"
          "<tt:Uri>" + rtsp + "</tt:Uri>"
          "<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
          "<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
          "<tt:Timeout>PT60S</tt:Timeout>"
          "</trt:MediaUri></trt:GetStreamUriResponse>");
    }

    // Операция не реализована в прототипе -> SOAP Fault (многие VMS это переживают).
    return envelope(
      "<s:Fault xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
      "<s:Code><s:Value>s:Receiver</s:Value></s:Code>"
      "<s:Reason><s:Text xml:lang=\"en\">NotImplemented</s:Text></s:Reason></s:Fault>");
}

bool OnvifService::start() {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    listenSock_ = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ < 0) return false;

    int reuse = 1;
    setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(cfg_.onvifPort);
    if (bind(listenSock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(listenSock_); listenSock_ = -1; return false;
    }
    listen(listenSock_, 8);

    running_ = true;
    worker_ = std::thread([this] { run(); });
    std::printf("[onvif] ONVIF на http://%s:%u/onvif/device_service\n",
                cfg_.localIp.c_str(), cfg_.onvifPort);
    return true;
}

void OnvifService::run() {
    while (running_) {
        sockaddr_in cli{}; socklen_t cl = sizeof(cli);
        int c = (int)accept(listenSock_, (sockaddr*)&cli, &cl);
        if (c < 0) continue;

        // Читаем HTTP-запрос целиком (прототип: один recv-цикл до пустой строки + тело).
        std::string req; char buf[4096]; int n;
        while ((n = recv(c, buf, sizeof(buf), 0)) > 0) {
            req.append(buf, n);
            if (req.size() > 65536) break;                 // защита от разрастания
            if (req.find("\r\n\r\n") != std::string::npos) {
                // Дочитываем тело по Content-Length, если оно ещё не всё пришло.
                auto hpos = req.find("\r\n\r\n") + 4;
                auto clp = req.find("Content-Length:");
                size_t need = 0;
                if (clp != std::string::npos)
                    need = (size_t)std::strtoul(req.c_str() + clp + 15, nullptr, 10);
                if (req.size() - hpos >= need) break;
            }
        }
        auto hpos = req.find("\r\n\r\n");
        std::string body = (hpos == std::string::npos) ? "" : req.substr(hpos + 4);

        // ПРИМЕЧАНИЕ: здесь место для проверки WS-Security UsernameToken (digest),
        // если cfg_.user не пуст. В MVP принимаем анонимно.

        std::string xml = dispatch(body);
        std::string resp = httpResponse(xml);
        send(c, resp.c_str(), (int)resp.size(), 0);
        closesocket(c);
    }
}

void OnvifService::stop() {
    if (!running_) return;
    running_ = false;
    if (listenSock_ >= 0) { closesocket(listenSock_); listenSock_ = -1; }
    if (worker_.joinable()) worker_.join();
}
