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
- `-DS2RTSP_BUILD_APP=OFF` собирает только ядро и тесты, GStreamer не нужен.
- Ручная проверка на Windows: VLC по `rtsp://<IP>:8554/stream`, затем ONVIF
  Device Manager. Линтера в репозитории нет.

Под Linux (Ubuntu 24.04, GStreamer 1.24) проект собирается и запускается;
захвата экрана там нет, поэтому приложение само включает тестовую картинку:

```bash
cmake -B build -S . && cmake --build build -j4
ctest --test-dir build --output-on-failure
```

- `unit` — модульные тесты (`tests/test_*.cpp`, каркас `tests/check.hpp`, без
  внешних зависимостей). Новая логика в ядре должна приходить вместе с тестом.
- `smoke_rtsp` — `tests/smoke_rtsp.sh`: запуск с тестовой картинкой, ONVIF
  `GetStreamUri`, приём кадров H.264 по RTSP.
- После любой правки запускать `ctest`; оба теста должны проходить.
- CI (`.github/workflows/ci.yml`) на каждый push: Linux — сборка и оба теста,
  Windows — сборка MSVC с официальными бинарниками GStreamer и `unit`.
- Захват экрана и аппаратное кодирование (`d3d11*`) нигде автоматически не
  проверяются: CI на Windows только компилирует. Правки в этой части без
  ручной проверки на Windows нужно явно помечать как непроверенные.
- `localIp` на Linux всегда `127.0.0.1`.

## Устройство кода

| Файл | Роль |
|------|------|
| `src/config.hpp` | структура `Config` со значениями по умолчанию |
| `src/main.cpp` | разбор аргументов, определение IPv4, запуск модулей, GLib main loop |
| `src/pipeline.*` | строка GStreamer-пайплайна: захват экрана или тестовая картинка |
| `src/media_server.*` | `gst-rtsp-server`; единственный модуль, которому нужен GStreamer |
| `src/onvif_service.*` | свой HTTP+SOAP сервер, ONVIF Device и Media |
| `src/ws_discovery.*` | ответчик WS-Discovery, UDP multicast 239.255.255.250:3702 |
| `src/net_compat.hpp` | прослойка Winsock/POSIX: заголовки, `netInit()`, `waitReadable()` |
| `tests/` | модульные тесты ядра и сквозная проверка RTSP |

CMake-цели: `s2rtsp_core` (статическая библиотека: ONVIF, WS-Discovery,
пайплайн — без GStreamer), `s2rtsp` (приложение), `s2rtsp_tests`.

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
- `parseArgs`: все ключи идут парами `--ключ значение`, кроме флага
  `--test-pattern`; `showCursor` и `streamPath` из командной строки не задаются.
- Фоновые потоки ONVIF и WS-Discovery ждут данные через `waitReadable()` с
  таймаутом и проверяют `running_`. Не заменять это блокирующим `accept` или
  `recvfrom`: на Linux закрытие сокета их не прерывает, и `stop()` зависнет.
- `OnvifService::dispatch()` и `WsDiscovery::handleMessage()` — чистые функции
  без работы с сетью, на них держатся модульные тесты.
- `s2rtsp_core` не должен зависеть от GStreamer, иначе тесты перестанут
  собираться без него.
- Тестовая картинка всегда кодируется `x264enc`, независимо от
  `S2RTSP_SOFTWARE_ENCODER`.
- После `g_main_loop_run` нет обработчика выхода: код остановки модулей ниже
  него по Ctrl+C не выполняется.

Известные заглушки (не считать багами без запроса): время в
`GetSystemDateAndTime` (готовая `utcNow()` не подключена), отсутствие проверки
WS-Security при заданных `--user/--pass`, разрешение 1920×1080 в ONVIF-ответах.

## Стиль

- Комментарии и сообщения в консоль — на русском, как в существующем коде.
- Отступ 4 пробела, `#pragma once`, поля класса с суффиксом `_`, вспомогательные
  функции — в анонимном `namespace` в `.cpp`.
- Сокеты хранятся как `int`; платформенные различия собраны в
  `src/net_compat.hpp`, а не разбросаны по `.cpp`.
- Зависимости подключаются через `pkg_check_modules` в `CMakeLists.txt`; при
  добавлении новой нужно обновить и команду `vcpkg install` в `README.md`.
