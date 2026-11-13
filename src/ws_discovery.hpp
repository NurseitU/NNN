#pragma once
#include "config.hpp"
#include <atomic>
#include <string>
#include <thread>

// WsDiscovery: минимальный ответчик WS-Discovery (ONVIF).
// Слушает multicast 239.255.255.250:3702, на Probe типа
// dn:NetworkVideoTransmitter отвечает ProbeMatch с XAddrs устройства.
// Без этого часть VMS не находит камеру автоматически (но можно добавить вручную по IP).
class WsDiscovery {
public:
    explicit WsDiscovery(const Config& cfg);
    ~WsDiscovery();

    bool start();   // поднимает UDP-сокет и фоновый поток
    void stop();

    // Разбор входящей датаграммы: возвращает текст ответа (ProbeMatch) или пустую
    // строку, если отвечать не нужно. Без работы с сетью — открыто для модульных тестов.
    std::string handleMessage(const std::string& xml) const;
    std::string buildProbeMatch(const std::string& relatesToMsgId) const;

private:
    void run();

    Config cfg_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    int sock_ = -1; // SOCKET на Windows приводится из int; см. .cpp
};
