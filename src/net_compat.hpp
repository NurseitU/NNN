#pragma once
// Общая прослойка над сокетами Winsock/POSIX для сетевых модулей и тестов.
// Сокеты в проекте хранятся как int; все платформенные различия собраны здесь.

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socklen_t = int;
#else
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define closesocket ::close
#endif

// Инициализация сетевой подсистемы (нужна только Winsock; повторные вызовы безопасны).
inline void netInit() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

// Ждёт данных на сокете не дольше timeoutMs.
// Фоновые потоки используют это вместо блокирующего accept/recvfrom, чтобы
// периодически проверять флаг остановки: закрытие сокета из другого потока
// на Linux блокирующий вызов не прерывает.
inline bool waitReadable(int sock, int timeoutMs) {
    fd_set rd;
    FD_ZERO(&rd);
#ifdef _WIN32
    FD_SET(static_cast<SOCKET>(sock), &rd);
#else
    FD_SET(sock, &rd);
#endif
    timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    return select(sock + 1, &rd, nullptr, nullptr, &tv) > 0;
}
