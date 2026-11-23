#!/usr/bin/env bash
set -euo pipefail

# End-to-end wall-clock performance comparison: mux vs tmuxinator on a 10-window x 4-pane project.
# Usage: MUX=./build/mux bash benchmarks/perf_compare.sh [runs]

MUX="${MUX:-./build/mux}"
RUNS="${1:-3}"
TMPDIR_BASE=$(mktemp -d)
SOCK="mux-perf-$$"

cleanup() {
    if command -v tmux >/dev/null 2>&1; then
        tmux -L "$SOCK" kill-server 2>/dev/null || true
    fi
    rm -rf "$TMPDIR_BASE"
}
trap cleanup EXIT

CFG="$TMPDIR_BASE/perf10x4.yml"
cat >"$CFG" <<EOF
name: perf10x4
root: $TMPDIR_BASE
attach: false
socket_name: $SOCK
pre_window: export PERF_TEST=1
windows:
EOF

for w in $(seq 1 10); do
    cat >>"$CFG" <<EOF
  - win_${w}:
      layout: tiled
      panes:
        - echo w${w}p1
        - echo w${w}p2
        - echo w${w}p3
        - echo w${w}p4
EOF
done

now_ms() {
    date +%s%3N
}

measure_avg() {
    local label="$1"
    shift
    local total=0
    for _ in $(seq 1 "$RUNS"); do
        local t0 t1 dt
        t0=$(now_ms)
        "$@" >/dev/null
        t1=$(now_ms)
        dt=$((t1 - t0))
        total=$((total + dt))
    done
    local avg=$((total / RUNS))
    printf "%-36s %6d ms avg (%d runs)\n" "$label" "$avg" "$RUNS"
}

echo "=== mux end-to-end performance (10 windows x 4 panes = 40 panes) ==="
echo "Script size (tmux):      $("$MUX" debug --backend tmux -p "$CFG" | wc -c | tr -d ' ') bytes"
echo "Script size (herdr):     $("$MUX" debug --backend herdr -p "$CFG" | wc -c | tr -d ' ') bytes"
echo "Script size (herdr-app): $("$MUX" debug --backend herdr-app -p "$CFG" | wc -c | tr -d ' ') bytes"
echo

measure_avg "mux debug (tmux)" "$MUX" debug --backend tmux -p "$CFG"
measure_avg "mux debug (herdr)" "$MUX" debug --backend herdr -p "$CFG"
measure_avg "mux debug (herdr-app)" "$MUX" debug --backend herdr-app -p "$CFG"

if command -v tmux >/dev/null 2>&1; then
    total_start=0
    total_reattach=0
    total_stop=0
    for _ in $(seq 1 "$RUNS"); do
        tmux -L "$SOCK" kill-server 2>/dev/null || true
        t0=$(now_ms)
        "$MUX" start --backend tmux -p "$CFG"
        t1=$(now_ms)
        total_start=$((total_start + t1 - t0))

        t0=$(now_ms)
        "$MUX" start --backend tmux -p "$CFG"
        t1=$(now_ms)
        total_reattach=$((total_reattach + t1 - t0))

        t0=$(now_ms)
        "$MUX" stop --backend tmux -p "$CFG"
        t1=$(now_ms)
        total_stop=$((total_stop + t1 - t0))
    done
    printf "%-36s %6d ms avg (%d runs)\n" "mux live tmux cold start" "$((total_start / RUNS))" "$RUNS"
    printf "%-36s %6d ms avg (%d runs)\n" "mux live tmux repeat start" "$((total_reattach / RUNS))" "$RUNS"
    printf "%-36s %6d ms avg (%d runs)\n" "mux live tmux stop" "$((total_stop / RUNS))" "$RUNS"

    if command -v tmuxinator >/dev/null 2>&1; then
        tx_start=0
        tx_reattach=0
        tx_stop=0
        for _ in $(seq 1 "$RUNS"); do
            tmux -L "$SOCK" kill-server 2>/dev/null || true
            t0=$(now_ms)
            tmuxinator start --suppress-tmux-version-warning=true -p "$CFG" </dev/null >/dev/null
            t1=$(now_ms)
            tx_start=$((tx_start + t1 - t0))

            t0=$(now_ms)
            tmuxinator start --suppress-tmux-version-warning=true -p "$CFG" </dev/null >/dev/null
            t1=$(now_ms)
            tx_reattach=$((tx_reattach + t1 - t0))

            t0=$(now_ms)
            tmuxinator stop --suppress-tmux-version-warning=true -p "$CFG" </dev/null >/dev/null
            t1=$(now_ms)
            tx_stop=$((tx_stop + t1 - t0))
        done
        printf "%-36s %6d ms avg (%d runs)\n" "tmuxinator live tmux cold start" "$((tx_start / RUNS))" "$RUNS"
        printf "%-36s %6d ms avg (%d runs)\n" "tmuxinator live tmux repeat start" "$((tx_reattach / RUNS))" "$RUNS"
        printf "%-36s %6d ms avg (%d runs)\n" "tmuxinator live tmux stop" "$((tx_stop / RUNS))" "$RUNS"
    else
        echo "(tmuxinator not on PATH; run via 'nix shell nixpkgs#tmux nixpkgs#tmuxinator' for head-to-head)"
    fi
else
    echo "(tmux not on PATH; skipping live tmux timings)"
fi
