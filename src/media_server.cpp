#include "media_server.hpp"

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <string>

namespace {

// Строит GStreamer-пайплайн: захват монитора -> H.264 -> RTP payloader.
// Элемент payloader ОБЯЗАН называться pay0 (требование gst-rtsp-server).
//
// Аппаратный путь (DXGI + HW-энкодер). Если d3d11h264enc недоступен на машине,
// определите макрос S2RTSP_SOFTWARE_ENCODER для software-пути (x264enc).
std::string buildPipeline(const Config& c) {
    const std::string fps = std::to_string(c.fps);
    const std::string br  = std::to_string(c.bitrateKbps);
    const std::string cur = c.showCursor ? "true" : "false";

#ifndef S2RTSP_SOFTWARE_ENCODER
    // Аппаратный кодер (меньше задержка, меньше CPU).
    return
        "d3d11screencapturesrc monitor-index=" + std::to_string(c.monitorIndex) +
        " show-cursor=" + cur +
        " ! video/x-raw(memory:D3D11Memory),framerate=" + fps + "/1"
        " ! d3d11convert"
        " ! d3d11h264enc bitrate=" + br +
        " ! h264parse config-interval=1"
        " ! rtph264pay name=pay0 pt=96";
#else
    // Программный кодер (универсальный фоллбэк).
    // key-int-max=fps => IDR раз в секунду, config-interval=1 => SPS/PPS в потоке.
    return
        "d3d11screencapturesrc monitor-index=" + std::to_string(c.monitorIndex) +
        " show-cursor=" + cur +
        " ! video/x-raw(memory:D3D11Memory),framerate=" + fps + "/1"
        " ! d3d11download ! videoconvert"
        " ! x264enc tune=zerolatency speed-preset=veryfast"
        " key-int-max=" + fps + " bitrate=" + br +
        " ! h264parse config-interval=1"
        " ! rtph264pay name=pay0 pt=96";
#endif
}

} // namespace

MediaServer::MediaServer(const Config& cfg) : cfg_(cfg) {}

bool MediaServer::start() {
    gst_init(nullptr, nullptr);

    GstRTSPServer* server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, std::to_string(cfg_.rtspPort).c_str());

    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory* factory = gst_rtsp_media_factory_new();

    const std::string launch = "( " + buildPipeline(cfg_) + " )";
    gst_rtsp_media_factory_set_launch(factory, launch.c_str());

    // shared=TRUE: один энкод на всех клиентов (а не отдельный пайплайн на сессию).
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    gst_rtsp_mount_points_add_factory(mounts, cfg_.streamPath.c_str(), factory);
    g_object_unref(mounts);

    // attach возвращает 0 при ошибке (например, порт занят).
    if (gst_rtsp_server_attach(server, nullptr) == 0) {
        g_printerr("[media] не удалось привязать RTSP к порту %u\n", cfg_.rtspPort);
        return false;
    }

    g_print("[media] RTSP: %s\n", streamUri().c_str());
    return true;
}

std::string MediaServer::streamUri() const {
    return "rtsp://" + cfg_.localIp + ":" + std::to_string(cfg_.rtspPort) + cfg_.streamPath;
}
