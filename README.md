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
| `src/media_server.*` | GStreamer-пайплайн захват→H.264→RTP + `gst-rtsp-server` |
| `src/onvif_service.*` | минимальный ONVIF Device+Media (встроенный HTTP+SOAP) |
| `src/ws_discovery.*` | ответчик WS-Discovery (UDP 3702) для авто-обнаружения |
| `src/main.cpp` | связывает модули, GLib main loop |

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

Откройте UDP 3702, TCP 8554 и порт ONVIF (8080) в брандмауэре.

## Проверка (поэтапно)
1. **RTSP**: в VLC → «Открыть URL» → `rtsp://<IP>:8554/stream`. Должен пойти экран.
2. **Обнаружение**: запустите *ONVIF Device Manager* — устройство должно появиться.
3. **VMS**: добавьте ONVIF-камеру; клиент сам вызовет `GetStreamUri` и подхватит поток.

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
