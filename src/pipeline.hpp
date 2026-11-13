#pragma once
#include "config.hpp"
#include <string>

// Строит строку GStreamer-пайплайна: источник видео -> H.264 -> RTP payloader.
// Вынесено из MediaServer в отдельный модуль без зависимости от GStreamer,
// чтобы строку можно было проверять модульными тестами.
//
// Элемент payloader ОБЯЗАН называться pay0 (требование gst-rtsp-server).
std::string buildPipeline(const Config& cfg);
