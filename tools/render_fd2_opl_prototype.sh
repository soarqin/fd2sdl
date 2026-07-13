#!/usr/bin/env bash
# 外部 libADLMIDI 原型；不向 fd2sdl 链接或复制 GPL/LGPL 代码。
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${FD2_ADLMIDI_WORK:-/tmp/fd2-adlmidi-prototype}
TRACK=${1:-1}
EMULATOR=${FD2_ADLMIDI_EMULATOR:---emu-nuked}
COMMIT=9760df4e0acfe27818dee38e786f3b1fea8c8e7f
SRC="$WORK/libADLMIDI"
BUILD="$WORK/build"
ASSETS="$WORK/assets"
XMI="$WORK/xmi"

mkdir -p "$WORK" "$ASSETS"
if [[ ! -d "$SRC/.git" ]]; then
    git clone https://github.com/Wohlstand/libADLMIDI.git "$SRC"
fi
git -C "$SRC" fetch --depth 1 origin "$COMMIT"
git -C "$SRC" checkout --detach "$COMMIT"
cp "$ROOT/original_game/SAMPLE.AD" "$ASSETS/SAMPLE.AD"
cp "$ROOT/original_game/SAMPLE.OPL" "$ASSETS/SAMPLE.OPL"
cat >"$ASSETS/fd2-banks.ini" <<'EOF'
[General]
banks = 1
[bank-0]
name = "FD2 SAMPLE.AD"
games = "Flame Dragon 2"
format = AIL
file = "SAMPLE.AD"
prefix = "FD2"
EOF

cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_GENADLDATA=ON \
    -DGENADLDATA_CUSTOM_BANKLIST="$ASSETS/fd2-banks.ini" \
    -DWITH_MIDIPLAY=ON -DMIDIPLAY_WAVE_ONLY=ON \
    -DMIDIPLAY_PREFER_SDL=ON -DWITH_XMI_SUPPORT=ON \
    -DUSE_DOSBOX_EMULATOR=ON -DUSE_NUKED_EMULATOR=ON \
    -DUSE_OPAL_EMULATOR=OFF -DUSE_JAVA_EMULATOR=OFF
# 上游生成目标尚未声明为静态库的完整依赖，显式先生成 inst_db.cpp。
cmake --build "$BUILD" --target gen-adldata-run -j1
cmake --build "$BUILD" -j2

rm -rf "$XMI"
python3 "$ROOT/tools/analyze_fd2_audio.py" --extract-music-dir "$XMI" >/dev/null
render_track() {
    local track_file=$1
    "$BUILD/adlmidiplay" "$track_file" "$EMULATOR" -vm 8 0 1
    echo "prototype WAV: $track_file.wav"
}
if [[ "$TRACK" == all ]]; then
    for track_file in "$XMI"/*.xmi; do render_track "$track_file"; done
else
    TRACK_FILE=$(printf '%s/track%02d.xmi' "$XMI" "$TRACK")
    [[ -f "$TRACK_FILE" ]] || {
        echo "FDMUS track $TRACK 不是有效 XMIDI" >&2
        exit 1
    }
    render_track "$TRACK_FILE"
fi
echo "注意：libADLMIDI 为 LGPL/GPL/MIT 混合许可；本脚本仅用于外部原型。"
