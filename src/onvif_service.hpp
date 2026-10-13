#pragma once
#include "config.hpp"
#include "media_server.hpp"
#include <atomic>
#include <thread>

// OnvifService: минимальный ONVIF Profile S (Device + Media) поверх
// собственного крошечного HTTP+SOAP сервера.
//
// Назначение прототипа: чтобы VMS/ONVIF Device Manager обнаружил устройство
// и получил RTSP-URL через GetStreamUri. Реализованы только операции,
// которые реально дёргает клиент при добавлении камеры.
//
// ПРОДАКШН-ПУТЬ (рекомендуется потом): сгенерировать сервер из официальных
// WSDL через gSOAP:
//   soapcpp2 -S -x -L -I<gsoap>/import onvif.h
// и реализовать __tds__*/__trt__* хендлеры, а этот класс заменить обёрткой
// над stdsoap2. Контракт (какие операции обязательны) — тот же.
class OnvifService {
public:
    OnvifService(const Config& cfg, const MediaServer& media);
    ~OnvifService();

    bool start();   // HTTP на cfg.onvifPort, пути /onvif/device_service и /onvif/media_service
    void stop();

private:
    void run();
    std::string dispatch(const std::string& soapBody) const; // выбор операции по имени элемента

    Config            cfg_;
    const MediaServer& media_;
    std::atomic<bool> running_{false};
    std::thread       worker_;
    int               listenSock_ = -1;
};
