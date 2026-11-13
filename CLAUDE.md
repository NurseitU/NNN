# s2rtsp

Прототип на C++17: захват одного монитора → H.264 → RTSP-поток + минимальный
ONVIF Profile S, чтобы VMS/NVR видели экран как IP-камеру. Целевая платформа —
только Windows (DXGI Desktop Duplication через GStreamer `d3d11screencapturesrc`).
Обзор, запуск и ограничения описаны в `README.md`.

## Жёсткое правило: clean-room

Код пишется с нуля. Не переносить сюда код, строки, идентификаторы или ресурсы
из сторонних программ того же назначения — только собственная реализация по
открытым спецификациям (ONVIF, WS-Discovery, RTSP).

## Сборка и проверка

Сборка рассчитана на Windows (vcpkg + Visual Studio):

```bat
vcpkg install gstreamer[rtsp-server]:x64-windows glib:x64-windows
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
build\Release\s2rtsp.exe --monitor 0 --fps 30 --bitrate 6000
```

- `-DS2RTSP_SOFTWARE_ENCODER=ON` переключает `d3d11h264enc` на `x264enc`.
- Тестов, линтера и CI в репозитории нет. Проверка ручная: VLC по
  `rtsp://<IP>:8554/stream`, затем ONVIF Device Manager.
- В облачном Linux-контейнере есть `cmake`, `g++`, `clang++`, но нет GStreamer,
  поэтому проект там не собирается и не запускается. Изменения, сделанные без
  сборки, нужно явно помечать как непроверенные.
- В сетевых модулях есть POSIX-ветки (`#else` у `_WIN32`), но сборка под Linux
  не проверялась, а захват экрана там не работает в принципе.

## Устройство кода

| Файл | Роль |
|------|------|
| `src/config.hpp` | структура `Config` со значениями по умолчанию |
| `src/main.cpp` | разбор аргументов, определение IPv4, запуск модулей, GLib main loop |
| `src/media_server.*` | строка GStreamer-пайплайна и `gst-rtsp-server` |
| `src/onvif_service.*` | свой HTTP+SOAP сервер, ONVIF Device и Media |
| `src/ws_discovery.*` | ответчик WS-Discovery, UDP multicast 239.255.255.250:3702 |

Порты по умолчанию: RTSP TCP 8554, ONVIF HTTP 8080, WS-Discovery UDP 3702.

## Что легко сломать

- Payloader в пайплайне обязан называться `pay0` — этого требует `gst-rtsp-server`.
- Поток начинает работать только внутри `g_main_loop_run`; до него сервер лишь
  смонтирован.
- Каждый модуль хранит свою копию `Config` (`cfg_` по значению). Всё, что
  вычисляется при старте (например, `localIp`), нужно записать в `Config` до
  создания модулей.
- `OnvifService::dispatch()` выбирает операцию поиском подстроки в теле запроса,
  сверху вниз. Новая операция — это новая ветка `has("ИмяОперации")`; имя не
  должно быть подстрокой уже обрабатываемого выше. Неизвестные операции
  получают SOAP Fault `NotImplemented`.
- Реализованы только `GetSystemDateAndTime`, `GetDeviceInformation`,
  `GetServices`/`GetCapabilities`, `GetScopes`, `GetProfiles`,
  `GetVideoSources`, `GetStreamUri`.
- XML собирается конкатенацией строк; значения из `Config` не экранируются.
- `parseArgs` читает аргументы только парами `--ключ значение`; `showCursor` и
  `streamPath` из командной строки не задаются.
- После `g_main_loop_run` нет обработчика выхода: код остановки модулей ниже
  него по Ctrl+C не выполняется.

Известные заглушки (не считать багами без запроса): время в
`GetSystemDateAndTime` (готовая `utcNow()` не подключена), отсутствие проверки
WS-Security при заданных `--user/--pass`, разрешение 1920×1080 в ONVIF-ответах.

## Стиль

- Комментарии и сообщения в консоль — на русском, как в существующем коде.
- Отступ 4 пробела, `#pragma once`, поля класса с суффиксом `_`, вспомогательные
  функции — в анонимном `namespace` в `.cpp`.
- Сокеты хранятся как `int`; платформенные различия закрываются блоками
  `#ifdef _WIN32` в начале `.cpp`, а не разбросаны по коду.
- Новые зависимости добавлять только через `pkg_check_modules` в
  `CMakeLists.txt` и одновременно в команду `vcpkg install` в `README.md`.
