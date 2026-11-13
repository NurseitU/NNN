#include "pipeline.hpp"

namespace {

// Общий хвост: упаковка H.264 в RTP. config-interval=1 => SPS/PPS в потоке.
const char kRtpTail[] =
    " ! h264parse config-interval=1"
    " ! rtph264pay name=pay0 pt=96";

// Программный кодер. key-int-max=fps => IDR раз в секунду.
std::string x264(const std::string& fps, const std::string& br) {
    return " ! x264enc tune=zerolatency speed-preset=veryfast"
           " key-int-max=" + fps + " bitrate=" + br;
}

} // namespace

std::string buildPipeline(const Config& c) {
    const std::string fps = std::to_string(c.fps);
    const std::string br  = std::to_string(c.bitrateKbps);
    const std::string cur = c.showCursor ? "true" : "false";

    if (c.testPattern) {
        // Тестовая картинка вместо захвата экрана: не требует Windows и D3D11,
        // поэтому весь тракт RTSP/ONVIF можно проверить на любой платформе.
        // Размер совпадает с разрешением, которое отдаётся в ONVIF-ответах.
        // Кодер всегда программный (нужен GStreamer-плагин x264).
        return
            "videotestsrc is-live=true"
            " ! video/x-raw,width=1920,height=1080,framerate=" + fps + "/1"
            " ! videoconvert" +
            x264(fps, br) +
            kRtpTail;
    }

    const std::string capture =
        "d3d11screencapturesrc monitor-index=" + std::to_string(c.monitorIndex) +
        " show-cursor=" + cur +
        " ! video/x-raw(memory:D3D11Memory),framerate=" + fps + "/1";

#ifndef S2RTSP_SOFTWARE_ENCODER
    // Аппаратный кодер (меньше задержка, меньше CPU).
    return capture +
        " ! d3d11convert"
        " ! d3d11h264enc bitrate=" + br +
        kRtpTail;
#else
    // Программный кодер (универсальный фоллбэк).
    return capture +
        " ! d3d11download ! videoconvert" +
        x264(fps, br) +
        kRtpTail;
#endif
}
