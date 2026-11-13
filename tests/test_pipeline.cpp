#include "check.hpp"
#include "pipeline.hpp"

// Payloader обязан называться pay0 — иначе gst-rtsp-server не отдаст поток.
TEST(pipeline_payloader_is_named_pay0) {
    Config cfg;
    CHECK_CONTAINS(buildPipeline(cfg), "rtph264pay name=pay0 pt=96");

    cfg.testPattern = true;
    CHECK_CONTAINS(buildPipeline(cfg), "rtph264pay name=pay0 pt=96");
}

TEST(pipeline_capture_uses_config_values) {
    Config cfg;
    cfg.monitorIndex = 2;
    cfg.fps = 25;
    cfg.bitrateKbps = 4000;
    cfg.showCursor = false;

    const std::string p = buildPipeline(cfg);
    CHECK_CONTAINS(p, "d3d11screencapturesrc monitor-index=2");
    CHECK_CONTAINS(p, "show-cursor=false");
    CHECK_CONTAINS(p, "framerate=25/1");
    CHECK_CONTAINS(p, "bitrate=4000");
    CHECK_NOT_CONTAINS(p, "videotestsrc");

#ifndef S2RTSP_SOFTWARE_ENCODER
    CHECK_CONTAINS(p, "d3d11h264enc");
    CHECK_NOT_CONTAINS(p, "x264enc");
#else
    CHECK_CONTAINS(p, "x264enc");
    CHECK_CONTAINS(p, "key-int-max=25");
    CHECK_NOT_CONTAINS(p, "d3d11h264enc");
#endif
}

// Тестовая картинка не должна зависеть от D3D11: её запускают там, где его нет.
TEST(pipeline_test_pattern_has_no_d3d11_elements) {
    Config cfg;
    cfg.testPattern = true;
    cfg.fps = 15;
    cfg.bitrateKbps = 1500;

    const std::string p = buildPipeline(cfg);
    CHECK_CONTAINS(p, "videotestsrc is-live=true");
    CHECK_CONTAINS(p, "width=1920,height=1080,framerate=15/1");
    CHECK_CONTAINS(p, "x264enc");
    CHECK_CONTAINS(p, "key-int-max=15");
    CHECK_CONTAINS(p, "bitrate=1500");
    CHECK_NOT_CONTAINS(p, "d3d11");
}
