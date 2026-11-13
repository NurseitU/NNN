#include "check.hpp"
#include "ws_discovery.hpp"

namespace {

const char kProbe[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
    " xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\""
    " xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">"
    "<e:Header>"
    "<w:MessageID>uuid:11111111-2222-3333-4444-555555555555</w:MessageID>"
    "<w:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>"
    "<w:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action>"
    "</e:Header>"
    "<e:Body><d:Probe><d:Types>dn:NetworkVideoTransmitter</d:Types></d:Probe></e:Body>"
    "</e:Envelope>";

Config testConfig() {
    Config cfg;
    cfg.localIp    = "192.0.2.10";
    cfg.onvifPort  = 8081;
    cfg.deviceName = "TestCam";
    cfg.model      = "TestModel";
    cfg.uuid       = "urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    return cfg;
}

} // namespace

TEST(wsd_probe_gets_probe_match) {
    const WsDiscovery wsd(testConfig());
    const std::string reply = wsd.handleMessage(kProbe);

    CHECK(!reply.empty());
    CHECK_CONTAINS(reply, "discovery/ProbeMatches</a:Action>");
    CHECK_CONTAINS(reply, "<d:Types>dn:NetworkVideoTransmitter</d:Types>");
}

// Клиент сопоставляет ответ с запросом по RelatesTo == MessageID запроса.
TEST(wsd_reply_relates_to_request_message_id) {
    const WsDiscovery wsd(testConfig());
    CHECK_CONTAINS(wsd.handleMessage(kProbe),
        "<a:RelatesTo>uuid:11111111-2222-3333-4444-555555555555</a:RelatesTo>");
}

TEST(wsd_reply_advertises_device_service_address) {
    const WsDiscovery wsd(testConfig());
    const std::string reply = wsd.handleMessage(kProbe);

    CHECK_CONTAINS(reply, "<d:XAddrs>http://192.0.2.10:8081/onvif/device_service</d:XAddrs>");
    CHECK_CONTAINS(reply,
        "<a:Address>urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee</a:Address>");
    CHECK_CONTAINS(reply, "onvif://www.onvif.org/name/TestCam");
    CHECK_CONTAINS(reply, "onvif://www.onvif.org/hardware/TestModel");
    CHECK_CONTAINS(reply, "onvif://www.onvif.org/Profile/Streaming");
}

TEST(wsd_probe_without_types_is_answered) {
    const WsDiscovery wsd(testConfig());
    const std::string probe =
        "<e:Envelope><e:Header><w:MessageID>uuid:42</w:MessageID></e:Header>"
        "<e:Body><d:Probe/></e:Body></e:Envelope>";
    CHECK_CONTAINS(wsd.handleMessage(probe), "<a:RelatesTo>uuid:42</a:RelatesTo>");
}

TEST(wsd_non_probe_messages_are_ignored) {
    const WsDiscovery wsd(testConfig());
    CHECK(wsd.handleMessage("").empty());
    CHECK(wsd.handleMessage("not xml at all").empty());
    CHECK(wsd.handleMessage(
        "<e:Envelope><e:Body><d:Hello><d:Types>dn:NetworkVideoTransmitter</d:Types>"
        "</d:Hello></e:Body></e:Envelope>").empty());
}
