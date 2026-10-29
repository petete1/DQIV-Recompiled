#!/usr/bin/env bash
# Runs a test copy of the game on a hidden X display (Xvfb), to drive it with
# keys and take screenshots, without touching dist/ or the player's desktop.
#
#   tests/screen/game.sh start [64|32]  pack build64 (or build32) into ~/.cache/dq4-screen,
#                                       with a copy of dist/saves, and start it windowed
#   tests/screen/game.sh keys ACTION... keys, waits and resizes (see xkeys.py)
#   tests/screen/game.sh shot FILE [WxH] screenshot of the top-left WxH (default 1280x720)
#   tests/screen/game.sh journal        the game's journal (errors, crashes)
#   tests/screen/game.sh stop           stop the game and the display
#
# Build first (cmake --build build64). Needs Xvfb, ffmpeg, libXtst and the game's
# data: packed in assets, or a ROM in ROMandOBBfileHere.
# Example: start, wait for the title, resize to 16:10, open the menu, screenshot:
#   tests/screen/game.sh start && tests/screen/game.sh keys wait:12000 resize:1920x1200 \
#       wait:8000 space wait:3000 space && tests/screen/game.sh shot /tmp/menu.png 1920x1200
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
here=$root/tests/screen
run=${DQ4_SCREEN_DIR:-$HOME/.cache/dq4-screen}
display=${DQ4_SCREEN_DISPLAY:-:79}

stop() {
    # By the saved process numbers only: other copies of the game may be running.
    for pid in $(cat "$run"/*.pid 2>/dev/null); do kill -9 "$pid" 2>/dev/null || true; done
    rm -f "$run"/*.pid
}

case ${1:-} in
start)
    build=$root/build${2:-64}
    [ -x "$build/DragonQuestIV" ] || { echo "Build $build first" >&2; exit 1; }
    stop
    rm -rf "$run"; mkdir -p "$run/saves"
    if [ -d "$root/assets/data" ]; then
        "$build/dqiv_pack" "$build/DragonQuestIV" "$root/assets" "$root/addons" "$run/DragonQuestIV" >/dev/null
    else
        "$build/dqiv_pack" "$build/DragonQuestIV" "$root/addons" "$run/DragonQuestIV" >/dev/null
        ln -s "$root/ROMandOBBfileHere" "$run/ROMandOBBfileHere"
    fi
    cp -r "$root/dist/saves/." "$run/saves/" 2>/dev/null || true
    rm -f "$run/saves/journal.txt"
    [ ! -f "$run/saves/settings.ini" ] || sed -i 's/^fullscreen=.*/fullscreen=0/' "$run/saves/settings.ini"
    # Large enough for ultrawide windows; without a window manager, fullscreen has no effect.
    Xvfb "$display" -screen 0 4000x1300x24 >/dev/null 2>&1 & echo $! > "$run/xvfb.pid"
    sleep 2
    (cd "$run" && DISPLAY=$display SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy ./DragonQuestIV >/dev/null 2>&1 & echo $! > "$run/game.pid")
    echo "Started in $run on $display"
    ;;
keys) shift; python3 -I "$here/xkeys.py" "$display" "$@" ;;
shot)
    ffmpeg -loglevel error -y -f x11grab -video_size "${3:-1280x720}" -i "$display+0,0" -frames:v 1 "$2"
    ;;
journal) cat "$run/saves/journal.txt" ;;
stop) stop ;;
*) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
