#pragma once
#include "config.hpp"
#include <string>

// MediaServer: захват одного монитора (DXGI Desktop Duplication) ->
// кодирование H.264 -> раздача по RTSP через gst-rtsp-server.
//
// Вся связка capture->encode->RTP->RTSP описана одной GStreamer-строкой,
// поэтому своего кода пакетизации/SDP не требуется: gst-rtsp-server
// формирует SDP (включая sprop-parameter-sets) автоматически.
class MediaServer {
public:
    explicit MediaServer(const Config& cfg);

    // Поднимает RTSP-сервер и монтирует фабрику на cfg.streamPath.
    // Возвращает false, если не удалось привязать порт.
    // ВАЖНО: реально поток начинает крутиться только внутри GLib main loop
    // (его запускает App через g_main_loop_run).
    bool start();

    // rtsp://<localIp>:<rtspPort><streamPath> — отдаётся наружу и
    // возвращается в ONVIF GetStreamUri.
    std::string streamUri() const;

private:
    Config cfg_;
};
