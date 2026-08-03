#!/usr/bin/env bash
#
# Over-the-air firmware / filesystem upload to the AD9852 DDS controller.
#
# Talks to the ElegantOTA endpoints served by the firmware (sync ESP8266WebServer):
#   GET  /ota/start?mode=<fr|fs>&hash=<md5>   -> begins the update
#   POST /ota/upload  (multipart file)        -> streams the binary, then reboots
#
# Usage:
#   tools/ota.sh [all|fw|fs]     (default: all)
#
# Config (env overrides):
#   OTA_HOST   target host           (default: ad9852.local)
#   OTA_USER   basic-auth username    (default: admin)
#   OTA_PASS   basic-auth password    (default: parsed from platformio.ini [ota])
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/.pio/build/esp07"
INI="$PROJECT_DIR/platformio.ini"

HOST="${OTA_HOST:-ad9852.local}"
USER="${OTA_USER:-admin}"
# Single source of truth for the password: the [ota] password key in platformio.ini
PASS="${OTA_PASS:-$(sed -n 's/^[[:space:]]*password[[:space:]]*=[[:space:]]*//p' "$INI" | head -1)}"

[ -n "$PASS" ] || { echo "ERROR: OTA password not found (set OTA_PASS or [ota] password in platformio.ini)"; exit 1; }

wait_online() {
    printf '    waiting for %s to come back' "$HOST"
    for _ in $(seq 1 60); do
        sleep 2
        if curl -fsS -o /dev/null --max-time 3 "http://$HOST/"; then
            printf ' — online\n'
            return 0
        fi
        printf '.'
    done
    printf ' — TIMEOUT\n'
    return 1
}

push() {
    local mode="$1" file="$2" label="$3"
    [ -f "$file" ] || { echo "ERROR: $file not found — build step failed?"; exit 1; }
    local md5 size
    md5="$(md5sum "$file" | cut -d' ' -f1)"
    size="$(wc -c < "$file")"
    echo "==> $label — $(basename "$file")  ($size bytes, md5 $md5)"

    echo "    /ota/start (mode=$mode)"
    curl -fsS -u "$USER:$PASS" "http://$HOST/ota/start?mode=$mode&hash=$md5" | sed 's/^/    -> /'
    echo

    echo "    /ota/upload"
    curl -fsS -u "$USER:$PASS" -F "file=@$file;filename=$(basename "$file")" \
        "http://$HOST/ota/upload" | sed 's/^/    -> /'
    echo "    upload accepted — device rebooting"
    wait_online
}

MODE="${1:-all}"
case "$MODE" in
    fw|firmware)
        pio run -e esp07
        push fr "$BUILD_DIR/firmware.bin" "Firmware"
        ;;
    fs|filesystem)
        pio run -e esp07 -t buildfs
        push fs "$BUILD_DIR/littlefs.bin" "Filesystem"
        ;;
    all)
        pio run -e esp07
        pio run -e esp07 -t buildfs
        push fr "$BUILD_DIR/firmware.bin"  "Firmware"
        push fs "$BUILD_DIR/littlefs.bin"  "Filesystem"
        ;;
    *)
        echo "Usage: $0 [all|fw|fs]"; exit 2 ;;
esac

echo "Done — $HOST updated ($MODE)."