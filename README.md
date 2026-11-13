# s2rtsp — прототип «экран → IP-камера (ONVIF/RTSP)»

Захватывает **один монитор целиком**, кодирует в **H.264** и отдаёт как
сетевую камеру: **RTSP**-поток + **ONVIF Profile S** (авто-обнаружение в VMS/NVR).
Без виртуальной веб-камеры. Платформа — **Windows** (DXGI Desktop Duplication).

> Это учебный прототип для изучения архитектуры и совместимости (clean-room):
> код написан с нуля, без заимствования из анализируемой программы.

## Что внутри
| Файл | Роль |
|------|------|
| `src/config.hpp` | параметры: монитор, fps, битрейт, порты, auth |
| `src/pipeline.*` | строка GStreamer-пайплайна захват→H.264→RTP (или тестовая картинка) |
| `src/media_server.*` | `gst-rtsp-server`, раздаёт пайплайн по RTSP |
| `src/onvif_service.*` | минимальный ONVIF Device+Media (встроенный HTTP+SOAP) |
| `src/ws_discovery.*` | ответчик WS-Discovery (UDP 3702) для авто-обнаружения |
| `src/net_compat.hpp` | прослойка над сокетами Winsock/POSIX |
| `src/main.cpp` | связывает модули, GLib main loop |
| `tests/` | модульные тесты и сквозная проверка RTSP |

## Архитектура
```
VMS ──Probe──▶ WS-Discovery ──ProbeMatch(XAddrs)──▶ VMS
VMS ──SOAP──▶ ONVIF Device/Media ──GetStreamUri──▶ rtsp://IP:8554/stream
VMS ──RTSP──▶ gst-rtsp-server ──RTP/H.264──▶ VMS
                 ▲
     d3d11screencapturesrc → d3d11convert → H.264 enc → rtph264pay(pay0)
```

## Сборка (Windows, vcpkg + Visual Studio)
```bat
vcpkg install gstreamer[rtsp-server]:x64-windows glib:x64-windows
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```
Если аппаратный кодер `d3d11h264enc` недоступен — соберите с программным:
```bat
cmake -B build -S . -DS2RTSP_SOFTWARE_ENCODER=ON -DCMAKE_TOOLCHAIN_FILE=...
```
> Нужны GStreamer-плагины: `d3d11`, `videoconvertscale`, `x264` (для software),
> `rtp`, `rtspclientsink`/`rtsp-server`. Проверка: `gst-inspect-1.0 d3d11screencapturesrc`.

## Запуск
```bat
build\Release\s2rtsp.exe --monitor 0 --fps 30 --bitrate 6000
```
Параметры: `--monitor N --fps N --bitrate K --rtsp-port P --onvif-port P --user U --pass P`.

Флаг `--test-pattern` заменяет захват экрана тестовой картинкой 1920×1080
(`videotestsrc` + `x264enc`, нужен GStreamer-плагин `x264`). Удобно, чтобы
проверить RTSP/ONVIF отдельно от захвата.

Откройте UDP 3702, TCP 8554 и порт ONVIF (8080) в брандмауэре.

## Проверка (поэтапно)
1. **RTSP**: в VLC → «Открыть URL» → `rtsp://<IP>:8554/stream`. Должен пойти экран.
2. **Обнаружение**: запустите *ONVIF Device Manager* — устройство должно появиться.
3. **VMS**: добавьте ONVIF-камеру; клиент сам вызовет `GetStreamUri` и подхватит поток.

## Сборка и запуск на Linux (только для проверки)
Захвата экрана на Linux нет, поэтому приложение там всегда отдаёт тестовую
картинку. Этого достаточно, чтобы проверять ONVIF, WS-Discovery и RTSP.
```bash
sudo apt-get install -y libgstreamer1.0-dev libgstrtspserver-1.0-dev libglib2.0-dev pkg-config \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-tools
cmake -B build -S . && cmake --build build -j4
./build/s2rtsp
```

## Тесты
```bash
ctest --test-dir build --output-on-failure
```
- `unit` — модульные тесты ONVIF, WS-Discovery и строки пайплайна
  (`tests/test_*.cpp`). GStreamer им не нужен: `-DS2RTSP_BUILD_APP=OFF` собирает
  только ядро и тесты.
- `smoke_rtsp` — `tests/smoke_rtsp.sh`: запускает приложение с тестовой
  картинкой, получает адрес потока через ONVIF `GetStreamUri` и принимает кадры
  H.264 по RTSP. Только Linux/macOS, нужны `curl` и `gst-launch-1.0`.

На Windows: `ctest --test-dir build -C Release --output-on-failure`.

CI (`.github/workflows/ci.yml`) на каждый push собирает проект и гоняет тесты
на Linux и собирает его MSVC на Windows.

## Известные ограничения прототипа
- Время в `GetSystemDateAndTime` — заглушка; строгие VMS могут требовать корректное UTC.
- WS-Security (UsernameToken digest) **не** проверяется — место отмечено в `onvif_service.cpp`.
- Разрешение в ONVIF-ответах зашито 1920×1080; подставьте реальное из параметров монитора.
- Защищённый DRM-контент при DXGI может отдаваться чёрным.
- Смена разрешения/DPI на лету требует пересоздания пайплайна (не реализовано).

## Переход на продакшн-ONVIF (gSOAP)
Встроенный SOAP-сервер покрывает добавление камеры, но для полной совместимости
сгенерируйте сервер из официальных WSDL:
```
soapcpp2 -S -x -L -I<gsoap>/import onvif.h
```
и реализуйте `__tds__*` / `__trt__*`; контракт операций — тот же, что в `dispatch()`.
