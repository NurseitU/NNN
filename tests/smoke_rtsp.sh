#!/usr/bin/env bash
# Сквозная проверка без Windows: запускает s2rtsp с тестовой картинкой,
# спрашивает адрес потока по ONVIF и принимает кадры H.264 по RTSP.
#
#   tests/smoke_rtsp.sh build/s2rtsp
#
# Нужны curl и gst-launch-1.0 с плагинами base/good/bad/ugly.
set -euo pipefail

BIN="${1:?usage: smoke_rtsp.sh <path-to-s2rtsp>}"
ONVIF_PORT="${ONVIF_PORT:-18080}"
RTSP_PORT="${RTSP_PORT:-18554}"
FRAMES="${FRAMES:-30}"
TIMEOUT="${TIMEOUT:-60}"   # секунд на приём; FRAMES / 15 должно быть заметно меньше

LOG="$(mktemp)"
"$BIN" --test-pattern --fps 15 --bitrate 1000 \
       --onvif-port "$ONVIF_PORT" --rtsp-port "$RTSP_PORT" >"$LOG" 2>&1 &
PID=$!
cleanup() {
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
    rm -f "$LOG"
}
trap cleanup EXIT

fail() {
    echo "FAIL: $1"
    echo "--- вывод s2rtsp ---"
    cat "$LOG"
    exit 1
}

# 1. Ждём, пока поднимется ONVIF, и спрашиваем адрес потока.
RESP=""
for _ in $(seq 1 50); do
    kill -0 "$PID" 2>/dev/null || fail "s2rtsp завершился при старте"
    RESP="$(curl -s --noproxy '*' -m 2 -X POST \
        -H 'Content-Type: application/soap+xml' \
        -d '<s:Envelope><s:Body><trt:GetStreamUri/></s:Body></s:Envelope>' \
        "http://127.0.0.1:${ONVIF_PORT}/onvif/media_service" || true)"
    [ -n "$RESP" ] && break
    sleep 0.2
done
[ -n "$RESP" ] || fail "ONVIF не ответил на порту ${ONVIF_PORT}"

URI="$(printf '%s' "$RESP" | sed -n 's:.*<tt\:Uri>\(.*\)</tt\:Uri>.*:\1:p')"
[ -n "$URI" ] || fail "в ответе GetStreamUri нет адреса: $RESP"
echo "ONVIF GetStreamUri -> $URI"

# 2. Принимаем FRAMES буферов H.264 по RTSP; fakesink завершит пайплайн сам.
if ! timeout "$TIMEOUT" gst-launch-1.0 -q \
        rtspsrc location="$URI" protocols=tcp latency=0 \
        ! rtph264depay ! h264parse \
        ! fakesink num-buffers="$FRAMES" sync=false; then
    fail "не удалось принять ${FRAMES} кадров по ${URI}"
fi

echo "OK: по RTSP принято ${FRAMES} буферов H.264"
