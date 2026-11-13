cmake_minimum_required(VERSION 3.20)
project(s2rtsp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опция: включить программный кодер (x264enc) вместо аппаратного d3d11h264enc.
option(S2RTSP_SOFTWARE_ENCODER "Use x264enc software encoder instead of d3d11h264enc" OFF)
# Опция: собирать само приложение (требует GStreamer). OFF => только ядро и тесты.
option(S2RTSP_BUILD_APP "Build the s2rtsp executable (requires GStreamer)" ON)
# Опция: собирать модульные тесты (GStreamer им не нужен).
option(S2RTSP_BUILD_TESTS "Build unit tests" ON)

if(MSVC)
    # Исходники в UTF-8 без BOM (русские комментарии и строки).
    add_compile_options(/utf-8)
    add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
endif()

find_package(Threads REQUIRED)

# --- Ядро: ONVIF, WS-Discovery и строка пайплайна. Без зависимости от GStreamer. ---
add_library(s2rtsp_core STATIC
    src/onvif_service.cpp
    src/ws_discovery.cpp
    src/pipeline.cpp
)
target_include_directories(s2rtsp_core PUBLIC src)
target_link_libraries(s2rtsp_core PUBLIC Threads::Threads)
if(S2RTSP_SOFTWARE_ENCODER)
    target_compile_definitions(s2rtsp_core PUBLIC S2RTSP_SOFTWARE_ENCODER=1)
endif()
if(WIN32)
    target_link_libraries(s2rtsp_core PUBLIC ws2_32 iphlpapi)
endif()

# --- Приложение ---
if(S2RTSP_BUILD_APP)
    # Зависимости GStreamer.
    # Через vcpkg: vcpkg install gstreamer[rtsp-server] glib
    # либо укажите PKG_CONFIG_PATH на установку GStreamer.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED
        gstreamer-1.0
        gstreamer-rtsp-server-1.0
        glib-2.0
        gobject-2.0
    )

    add_executable(s2rtsp
        src/main.cpp
        src/media_server.cpp
    )
    target_include_directories(s2rtsp PRIVATE ${GST_INCLUDE_DIRS})
    target_link_directories(s2rtsp PRIVATE ${GST_LIBRARY_DIRS})
    target_link_libraries(s2rtsp PRIVATE s2rtsp_core ${GST_LIBRARIES})
endif()

# --- Тесты ---
if(S2RTSP_BUILD_TESTS)
    enable_testing()

    add_executable(s2rtsp_tests
        tests/test_main.cpp
        tests/test_onvif.cpp
        tests/test_ws_discovery.cpp
        tests/test_pipeline.cpp
    )
    target_link_libraries(s2rtsp_tests PRIVATE s2rtsp_core)
    add_test(NAME unit COMMAND s2rtsp_tests)

    # Сквозная проверка: запуск приложения с тестовой картинкой, запрос ONVIF
    # и приём RTSP-потока. Нужны bash, curl и gst-launch-1.0 (не Windows).
    if(S2RTSP_BUILD_APP AND UNIX)
        find_program(GST_LAUNCH gst-launch-1.0)
        find_program(CURL_EXE curl)
        if(GST_LAUNCH AND CURL_EXE)
            add_test(NAME smoke_rtsp
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_rtsp.sh $<TARGET_FILE:s2rtsp>)
            set_tests_properties(smoke_rtsp PROPERTIES TIMEOUT 120)
        endif()
    endif()
endif()
