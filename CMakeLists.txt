cmake_minimum_required(VERSION 3.20)
project(s2rtsp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опция: включить программный кодер (x264enc) вместо аппаратного d3d11h264enc.
option(S2RTSP_SOFTWARE_ENCODER "Use x264enc software encoder instead of d3d11h264enc" OFF)

# --- Зависимости GStreamer ---
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
    src/onvif_service.cpp
    src/ws_discovery.cpp
)

target_include_directories(s2rtsp PRIVATE ${GST_INCLUDE_DIRS} src)
target_link_directories(s2rtsp PRIVATE ${GST_LIBRARY_DIRS})
target_link_libraries(s2rtsp PRIVATE ${GST_LIBRARIES})

if(S2RTSP_SOFTWARE_ENCODER)
    target_compile_definitions(s2rtsp PRIVATE S2RTSP_SOFTWARE_ENCODER=1)
endif()

if(WIN32)
    target_link_libraries(s2rtsp PRIVATE ws2_32 iphlpapi)
endif()
