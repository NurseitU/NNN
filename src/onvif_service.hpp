#pragma once
#include "config.hpp"
#include <atomic>
#include <string>
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
    // streamUri — RTSP-адрес потока (MediaServer::streamUri()), отдаётся в GetStreamUri.
    // Передаётся строкой, чтобы модуль не зависел от GStreamer и собирался для тестов.
    OnvifService(const Config& cfg, std::string streamUri);
    ~OnvifService();

    bool start();   // HTTP на cfg.onvifPort, пути /onvif/device_service и /onvif/media_service
    void stop();

    // Выбор операции по имени элемента в теле запроса; возвращает SOAP-ответ.
    // Чистая функция без работы с сетью — открыта для модульных тестов.
    std::string dispatch(const std::string& soapBody) const;

private:
    void run();

    Config            cfg_;
    std::string       streamUri_;
    std::atomic<bool> running_{false};
    std::thread       worker_;
    int               listenSock_ = -1;
};
