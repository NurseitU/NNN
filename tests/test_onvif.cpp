#include "check.hpp"
#include "net_compat.hpp"
#include "onvif_service.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace {

const char kStreamUri[] = "rtsp://192.0.2.10:8554/stream";

Config testConfig() {
    Config cfg;
    cfg.localIp      = "192.0.2.10";
    cfg.onvifPort    = 8081;
    cfg.fps          = 25;
    cfg.bitrateKbps  = 4000;
    cfg.manufacturer = "TestVendor";
    cfg.model        = "TestModel";
    cfg.firmware     = "9.9.9";
    cfg.serial       = "SN-TEST";
    cfg.deviceName   = "TestCam";
    return cfg;
}

std::string soap(const std::string& operation) {
    return "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
           "<s:Body><" + operation + "/></s:Body></s:Envelope>";
}

// --- Мини-клиент HTTP для проверки сервера по настоящему сокету ---

int connectLoopback(uint16_t port) {
    netInit();
    const int s = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(s);
        return -1;
    }
    return s;
}

// Читает ответ до закрытия соединения сервером (но не дольше ~5 секунд).
std::string readAll(int s) {
    std::string out;
    char buf[4096];
    while (waitReadable(s, 5000)) {
        const int n = (int)recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        out.append(buf, n);
    }
    return out;
}

std::string httpPost(const std::string& body) {
    return "POST /onvif/device_service HTTP/1.1\r\n"
           "Host: 127.0.0.1\r\n"
           "Content-Type: application/soap+xml; charset=utf-8\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "\r\n" + body;
}

} // namespace

// ---------- dispatch(): чистая логика без сети ----------

TEST(onvif_get_stream_uri_returns_rtsp_address) {
    const OnvifService svc(testConfig(), kStreamUri);
    const std::string r = svc.dispatch(soap("trt:GetStreamUri"));
    CHECK_CONTAINS(r, "<trt:GetStreamUriResponse>");
    CHECK_CONTAINS(r, "<tt:Uri>rtsp://192.0.2.10:8554/stream</tt:Uri>");
}

TEST(onvif_get_device_information_uses_config) {
    const OnvifService svc(testConfig(), kStreamUri);
    const std::string r = svc.dispatch(soap("tds:GetDeviceInformation"));
    CHECK_CONTAINS(r, "<tds:Manufacturer>TestVendor</tds:Manufacturer>");
    CHECK_CONTAINS(r, "<tds:Model>TestModel</tds:Model>");
    CHECK_CONTAINS(r, "<tds:FirmwareVersion>9.9.9</tds:FirmwareVersion>");
    CHECK_CONTAINS(r, "<tds:SerialNumber>SN-TEST</tds:SerialNumber>");
}

TEST(onvif_services_point_to_device_and_media_addresses) {
    const OnvifService svc(testConfig(), kStreamUri);
    for (const char* op : {"tds:GetServices", "tds:GetCapabilities"}) {
        const std::string r = svc.dispatch(soap(op));
        CHECK_CONTAINS(r, "<tds:XAddr>http://192.0.2.10:8081/onvif/device_service</tds:XAddr>");
        CHECK_CONTAINS(r, "<tds:XAddr>http://192.0.2.10:8081/onvif/media_service</tds:XAddr>");
    }
}

TEST(onvif_get_scopes_contains_profile_and_name) {
    const OnvifService svc(testConfig(), kStreamUri);
    const std::string r = svc.dispatch(soap("tds:GetScopes"));
    CHECK_CONTAINS(r, "onvif://www.onvif.org/Profile/Streaming");
    CHECK_CONTAINS(r, "onvif://www.onvif.org/name/TestCam");
}

TEST(onvif_get_profiles_describes_h264_main_stream) {
    const OnvifService svc(testConfig(), kStreamUri);
    const std::string r = svc.dispatch(soap("trt:GetProfiles"));
    CHECK_CONTAINS(r, "<trt:Profiles token=\"prof0\"");
    CHECK_CONTAINS(r, "<tt:Encoding>H264</tt:Encoding>");
    CHECK_CONTAINS(r, "<tt:FrameRateLimit>25</tt:FrameRateLimit>");
    CHECK_CONTAINS(r, "<tt:BitrateLimit>4000</tt:BitrateLimit>");
    CHECK_CONTAINS(r, "<tt:GovLength>25</tt:GovLength>");
}

TEST(onvif_get_video_sources_reports_framerate) {
    const OnvifService svc(testConfig(), kStreamUri);
    const std::string r = svc.dispatch(soap("trt:GetVideoSources"));
    CHECK_CONTAINS(r, "<trt:VideoSources token=\"vs0\">");
    CHECK_CONTAINS(r, "<tt:Framerate>25</tt:Framerate>");
}

TEST(onvif_get_system_date_and_time_is_answered) {
    const OnvifService svc(testConfig(), kStreamUri);
    CHECK_CONTAINS(svc.dispatch(soap("tds:GetSystemDateAndTime")),
                   "<tds:GetSystemDateAndTimeResponse>");
}

TEST(onvif_unknown_operation_returns_soap_fault) {
    const OnvifService svc(testConfig(), kStreamUri);
    for (const std::string& body : {soap("tds:SetHostname"), std::string()}) {
        const std::string r = svc.dispatch(body);
        CHECK_CONTAINS(r, "<s:Fault");
        CHECK_CONTAINS(r, "NotImplemented");
    }
}

TEST(onvif_every_response_is_a_soap_envelope) {
    const OnvifService svc(testConfig(), kStreamUri);
    for (const char* op : {"GetSystemDateAndTime", "GetDeviceInformation", "GetServices",
                           "GetScopes", "GetProfiles", "GetVideoSources", "GetStreamUri",
                           "SomethingElse"}) {
        const std::string r = svc.dispatch(soap(op));
        CHECK(r.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?><s:Envelope", 0) == 0);
        CHECK_CONTAINS(r, "</s:Body></s:Envelope>");
    }
}

// ---------- HTTP-сервер: запрос по настоящему сокету ----------

TEST(onvif_http_request_gets_soap_response) {
    Config cfg = testConfig();
    cfg.onvifPort = 18581;
    OnvifService svc(cfg, kStreamUri);
    CHECK(svc.start());

    const int s = connectLoopback(cfg.onvifPort);
    CHECK(s >= 0);
    if (s >= 0) {
        const std::string req = httpPost(soap("trt:GetStreamUri"));
        send(s, req.c_str(), (int)req.size(), 0);
        const std::string resp = readAll(s);
        closesocket(s);

        CHECK(resp.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
        CHECK_CONTAINS(resp, "Content-Type: application/soap+xml");
        CHECK_CONTAINS(resp, "<tt:Uri>rtsp://192.0.2.10:8554/stream</tt:Uri>");

        // Content-Length должен совпадать с фактической длиной тела.
        const size_t hdrEnd = resp.find("\r\n\r\n");
        CHECK(hdrEnd != std::string::npos);
        if (hdrEnd != std::string::npos) {
            const size_t bodyLen = resp.size() - (hdrEnd + 4);
            CHECK_CONTAINS(resp, "Content-Length: " + std::to_string(bodyLen) + "\r\n");
        }
    }
    svc.stop();
}

// Тело может прийти отдельным пакетом после заголовков: сервер обязан дочитать
// его по Content-Length, а не отвечать на пустое тело.
TEST(onvif_http_body_sent_after_headers_is_read) {
    Config cfg = testConfig();
    cfg.onvifPort = 18582;
    OnvifService svc(cfg, kStreamUri);
    CHECK(svc.start());

    const int s = connectLoopback(cfg.onvifPort);
    CHECK(s >= 0);
    if (s >= 0) {
        const std::string req = httpPost(soap("tds:GetDeviceInformation"));
        const size_t split = req.find("\r\n\r\n") + 4;
        send(s, req.c_str(), (int)split, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        send(s, req.c_str() + split, (int)(req.size() - split), 0);

        const std::string resp = readAll(s);
        closesocket(s);
        CHECK_CONTAINS(resp, "<tds:Manufacturer>TestVendor</tds:Manufacturer>");
        CHECK_NOT_CONTAINS(resp, "NotImplemented");
    }
    svc.stop();
}

TEST(onvif_serves_several_requests_in_a_row) {
    Config cfg = testConfig();
    cfg.onvifPort = 18583;
    OnvifService svc(cfg, kStreamUri);
    CHECK(svc.start());

    for (const char* op : {"GetProfiles", "GetStreamUri", "GetScopes"}) {
        const int s = connectLoopback(cfg.onvifPort);
        CHECK(s >= 0);
        if (s < 0) continue;
        const std::string req = httpPost(soap(op));
        send(s, req.c_str(), (int)req.size(), 0);
        CHECK_CONTAINS(readAll(s), std::string(op) + "Response>");
        closesocket(s);
    }
    svc.stop();
}

// stop() должен завершать фоновый поток, а не зависать, и после него порт свободен.
TEST(onvif_stop_returns_and_port_can_be_reused) {
    Config cfg = testConfig();
    cfg.onvifPort = 18584;
    {
        OnvifService first(cfg, kStreamUri);
        CHECK(first.start());
    } // деструктор вызывает stop()

    OnvifService second(cfg, kStreamUri);
    CHECK(second.start());
    second.stop();
    second.stop(); // повторный вызов безопасен
}

TEST(onvif_start_fails_when_port_is_taken) {
    Config cfg = testConfig();
    cfg.onvifPort = 18585;
    OnvifService first(cfg, kStreamUri);
    CHECK(first.start());

    OnvifService second(cfg, kStreamUri);
#ifndef _WIN32
    // На Windows SO_REUSEADDR разрешает повторную привязку к занятому порту,
    // поэтому отказ проверяется только на POSIX.
    CHECK(!second.start());
#endif
    first.stop();
}
