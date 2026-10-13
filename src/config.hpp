#pragma once
#include <string>
#include <cstdint>

// Конфигурация прототипа "экран -> IP-камера (ONVIF/RTSP)".
// Один монитор целиком, вывод H.264 по RTSP, обнаружение по ONVIF Profile S.
struct Config {
    int      monitorIndex = 0;      // индекс монитора (0 = основной)
    int      fps          = 30;     // частота кадров
    int      bitrateKbps  = 6000;   // битрейт H.264, кбит/с
    bool     showCursor   = true;   // рисовать курсор в потоке

    uint16_t rtspPort     = 8554;   // RTSP (TCP)
    uint16_t onvifPort    = 8080;   // ONVIF Device/Media SOAP (HTTP)
    std::string streamPath = "/stream";

    // Авторизация ONVIF (WS-Security UsernameToken digest).
    // Пусто => анонимно (только для MVP/локальной сети).
    std::string user;
    std::string pass;

    // Определяется при старте (IPv4 основного интерфейса).
    std::string localIp = "127.0.0.1";

    // Идентификация устройства для ONVIF GetDeviceInformation / Scopes.
    std::string manufacturer = "Prototype";
    std::string model        = "Screen2RTSP";
    std::string firmware     = "0.1.0";
    std::string serial       = "SN-0001";
    std::string deviceName   = "ScreenCam";
    std::string uuid         = "urn:uuid:3fa85f64-5717-4562-b3fc-2c963f66afa6"; // замените своим
};
