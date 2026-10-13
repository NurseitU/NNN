#pragma once
#include "config.hpp"
#include <atomic>
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

private:
    void run();
    std::string buildProbeMatch(const std::string& relatesToMsgId) const;

    Config cfg_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    int sock_ = -1; // SOCKET на Windows приводится из int; см. .cpp
};
