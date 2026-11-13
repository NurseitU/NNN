#include "media_server.hpp"
#include "pipeline.hpp"

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <string>

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
