#!/bin/sh
# 通过 Xvfb + XTest 驱动正式 --field-play 0 SDL 输入循环。
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 INPUT_DRIVER FD2SDL PROJECT_ROOT" >&2
    exit 2
fi

driver=$1
game=$2
project_root=$3
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/fd2-field-input.XXXXXX")
display_num=$((120 + ($$ % 400)))
while [ -e "/tmp/.X${display_num}-lock" ]; do
    display_num=$((display_num + 1))
done
display=":${display_num}"
xvfb_pid=
game_pid=

cleanup() {
    if [ -n "$game_pid" ]; then
        kill -KILL "$game_pid" 2>/dev/null || true
        wait "$game_pid" 2>/dev/null || true
    fi
    if [ -n "$xvfb_pid" ]; then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

Xvfb "$display" -screen 0 1024x768x24 >"$tmpdir/xvfb.log" 2>&1 &
xvfb_pid=$!
sleep 1

wait_for_log() {
    log=$1
    text=$2
    count=0
    while [ "$count" -lt 300 ]; do
        if grep -Fq "$text" "$log" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
        count=$((count + 1))
    done
    return 1
}

stop_game() {
    kill -TERM "$game_pid" 2>/dev/null || true
    count=0
    while kill -0 "$game_pid" 2>/dev/null && [ "$count" -lt 50 ]; do
        sleep 0.1
        count=$((count + 1))
    done
    kill -KILL "$game_pid" 2>/dev/null || true
    wait "$game_pid" 2>/dev/null || true
    game_pid=
}

run_case() {
    mode=$1
    log=$2
    seed=${3:-}
    expected=$4
    (
        cd "$project_root"
        if [ -n "$seed" ]; then
            DISPLAY="$display" SDL_VIDEODRIVER=x11 \
                SDL_AUDIODRIVER=dummy FD2SDL_FIELD_RNG_SEED="$seed" \
                "$game" --field-play 0
        else
            DISPLAY="$display" SDL_VIDEODRIVER=x11 \
                SDL_AUDIODRIVER=dummy "$game" --field-play 0
        fi
    ) >"$log" 2>&1 &
    game_pid=$!
    if ! wait_for_log "$log" 'field play: stage=0'; then
        cat "$log" >&2
        return 1
    fi
    DISPLAY="$display" "$driver" "$mode"
    if ! wait_for_log "$log" "$expected"; then
        cat "$log" >&2
        return 1
    fi
    stop_game
}

wait_log="$tmpdir/wait.log"
run_case wait "$wait_log" '' 'field phase: turn=2 phase=2 remaining=4'
grep -Fq 'field cursor: cell=(7,21)' "$wait_log"
grep -Fq 'field action: unit=0 command=wait acted=1 remaining=3 phase=2' "$wait_log"
grep -Fq 'field action: unit=1 command=wait acted=1 remaining=2 phase=2' "$wait_log"
grep -Fq 'field action: unit=2 command=wait acted=1 remaining=1 phase=2' "$wait_log"
grep -Fq 'field action: unit=3 command=wait acted=1 remaining=0 phase=1' "$wait_log"
grep -Fq 'field acted gate: unit=0, interaction=browse' "$wait_log"
grep -Fq 'field phase: turn=1 phase=1 remaining=0' "$wait_log"
grep -Fq 'field phase: turn=1 phase=0 remaining=8' "$wait_log"
grep -Fq 'field phase: turn=2 phase=2 remaining=4' "$wait_log"

attack_log="$tmpdir/attack.log"
run_case attack "$attack_log" '' 'field acted gate: unit=2, interaction=browse'
grep -Fq 'field action: unit=2 command=attack acted=1 remaining=3 phase=2' "$attack_log"
grep -Fq 'field attack: target=10 strikes=1 hit=1 critical=0 base=24 rolled=25 damage=25 total=25 hp=3 defeated=0 hidden=0' "$attack_log"
grep -Fq 'field acted gate: unit=2, interaction=browse' "$attack_log"

# seed 496 触发原版 3% 双击；不改 stage 0 坐标、HP 或攻防属性，
# 第二击把同一真实敌人从剩余 4 HP 提交为隐藏死亡。
lethal_log="$tmpdir/lethal.log"
run_case attack "$lethal_log" 496 'field acted gate: unit=2, interaction=browse'
grep -Fq 'field action: unit=2 command=attack acted=1 remaining=3 phase=2' "$lethal_log"
grep -Fq 'field attack: target=10 strikes=2 hit=1 critical=0 base=24 rolled=24 damage=4 total=28 hp=0 defeated=1 hidden=1' "$lethal_log"
grep -Fq 'field acted gate: unit=2, interaction=browse' "$lethal_log"
