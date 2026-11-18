#!/bin/sh
# 由 CTest 的 xvfb-run 提供本轮独占显示；不使用用户桌面。
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 INPUT_DRIVER FD2SDL PROJECT_ROOT" >&2
    exit 2
fi

driver=$1
game=$2
project_root=$3
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/fd2-field-input.XXXXXX")
game_pid=

cleanup() {
    if [ -n "$game_pid" ]; then
        if kill -0 "$game_pid" 2>/dev/null; then
            "$driver" quit || true
            count=0
            while kill -0 "$game_pid" 2>/dev/null && [ "$count" -lt 30 ]; do
                sleep 0.1
                count=$((count + 1))
            done
            kill -KILL "$game_pid" 2>/dev/null || true
        fi
        wait "$game_pid" 2>/dev/null || true
    fi
    rm -rf "$tmpdir"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

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
    "$driver" quit
    count=0
    while kill -0 "$game_pid" 2>/dev/null && [ "$count" -lt 30 ]; do
        sleep 0.1
        count=$((count + 1))
    done
    if kill -0 "$game_pid" 2>/dev/null; then
        echo 'game did not exit normally' >&2
        return 1
    fi
    status=0
    wait "$game_pid" || status=$?
    game_pid=
    return "$status"
}

run_case() {
    mode=$1
    log=$2
    seed=${3:-}
    expected=$4
    (
        cd "$project_root"
        unset FD2SDL_FIELD_RNG_SEED FD2SDL_APP_PCM_CAPTURE_DIR
        if [ -n "$seed" ]; then
            export FD2SDL_FIELD_RNG_SEED="$seed"
        fi
        export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy FD2SDL_SAVE_PATH="$tmpdir/FD2.SAV"
        exec "$game" --field-play 0
    ) >"$log" 2>&1 &
    game_pid=$!
    if ! wait_for_log "$log" 'field play: stage=0'; then
        cat "$log" >&2
        return 1
    fi
    "$driver" "$mode"
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
# 原1b750派生后敌raw10防御4；攻击26经地形+5%=27，原2ebe1/2f7b6基础伤害20。
grep -Fq 'field attack: target=10 strikes=1 hit=1 critical=0 base=20 rolled=21 damage=21 total=21 hp=7 defeated=0 hidden=0' "$attack_log"
grep -Fq 'field acted gate: unit=2, interaction=browse' "$attack_log"

# seed 496 触发原版 3% 双击；不改 stage 0 坐标、HP 或攻防属性，
# 第二击把同一真实敌人从剩余 8 HP 提交为隐藏死亡。
lethal_log="$tmpdir/lethal.log"
run_case attack "$lethal_log" 496 'field acted gate: unit=2, interaction=browse'
grep -Fq 'field action: unit=2 command=attack acted=1 remaining=3 phase=2' "$lethal_log"
grep -Fq 'field attack: target=10 strikes=2 hit=1 critical=0 base=20 rolled=20 damage=8 total=28 hp=0 defeated=1 hidden=1' "$lethal_log"
grep -Fq 'field acted gate: unit=2, interaction=browse' "$lethal_log"
