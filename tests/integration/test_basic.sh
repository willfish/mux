#!/usr/bin/env bash
set -euo pipefail

MUX="${MUX:-./build/mux}"
PASS=0
FAIL=0

# Resolve symlinks (macOS /var -> /private/var) so it matches tmux cwd paths.
TMPDIR_BASE=$(cd "$(mktemp -d)" && pwd -P)
SOCK="mux-itest-$$"

cleanup() {
    if command -v tmux >/dev/null 2>&1; then
        tmux -L "$SOCK" kill-server 2>/dev/null || true
    fi
    if [ -n "${SPACE_DIR:-}" ]; then
        rmdir "$SPACE_DIR" 2>/dev/null || true
    fi
    rm -rf "$TMPDIR_BASE"
}
trap cleanup EXIT

pass() {
    echo "  PASS: $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "  FAIL: $1"
    FAIL=$((FAIL + 1))
}

assert_eq() {
    local expected="$1"
    local actual="$2"
    local label="$3"
    if [ "$expected" = "$actual" ]; then
        pass "$label"
    else
        echo "    expected: [$expected]"
        echo "    actual:   [$actual]"
        fail "$label"
    fi
}

export TMUXINATOR_CONFIG="$TMPDIR_BASE/configs"
export MUX_BACKEND=tmux
unset EDITOR
mkdir -p "$TMUXINATOR_CONFIG"

echo "Running self-contained integration tests (MUX=$MUX)..."
echo

# 1. Version & help
echo "# CLI basics"
if "$MUX" version | grep -Fq "mux 0.2.0"; then
    pass "version reports 0.2.0"
else
    fail "version output"
fi

if "$MUX" help | grep -Fq "Usage:"; then
    pass "help output contains Usage"
else
    fail "help output"
fi

if "$MUX" start --definitely-unknown-flag work >/dev/null 2>&1; then
    fail "unknown flag should exit non-zero"
else
    pass "unknown flag fails cleanly"
fi

# 2. Completions syntax validation
echo "# completions"
if "$MUX" completions bash | bash -n; then
    pass "bash completions pass bash -n"
else
    fail "bash completions syntax"
fi

if "$MUX" completions zsh | grep -Fq "compdef _mux mux"; then
    pass "zsh completions register compdef"
else
    fail "zsh completions registration"
fi

if "$MUX" completions fish | grep -Fq "' -a '(mux list 2>/dev/null)'"; then
    pass "fish completions have valid -a spacing"
else
    fail "fish completions spacing"
fi

if command -v fish >/dev/null 2>&1; then
    if "$MUX" completions fish | fish -n; then
        pass "fish completions pass fish -n"
    else
        fail "fish completions fish -n"
    fi
fi

# 3. Generated script bash -n checks across all fixtures and backends
echo "# generated script syntax"
for backend in tmux herdr herdr-app; do
    if "$MUX" debug --backend "$backend" -p tests/fixtures/sample.yml | bash -n; then
        pass "$backend backend sample.yml passes bash -n"
    else
        fail "$backend backend sample.yml bash -n"
    fi
done

# 4. Real tmux end-to-end tests (isolated socket)
if ! command -v tmux >/dev/null 2>&1; then
    echo "# skipping live tmux tests (tmux not on PATH)"
else
    echo "# live tmux tests (socket: $SOCK)"

    # A previous step may have killed the server's last session; a client
    # connecting while that server exits gets "server exited unexpectedly".
    tmux_new_session() {
        tmux -L "$SOCK" new-session "$@" 2>/dev/null ||
            {
                sleep 0.2
                tmux -L "$SOCK" new-session "$@"
            }
    }

    # 4a. Session prefix isolation: work vs work-api
    tmux_new_session -d -s work-api
    cat >"$TMUXINATOR_CONFIG/work.yml" <<EOF
name: work
attach: false
socket_name: $SOCK
windows:
  - main: echo hello
EOF
    "$MUX" start work
    SESSIONS=$(tmux -L "$SOCK" list-sessions -F '#S' | sort | tr '\n' ',')
    assert_eq "work,work-api," "$SESSIONS" "start work does not collide with work-api"

    "$MUX" stop work
    SESSIONS=$(tmux -L "$SOCK" list-sessions -F '#S' | sort | tr '\n' ',')
    assert_eq "work-api," "$SESSIONS" "stop work does not kill work-api"
    tmux -L "$SOCK" kill-session -t =work-api

    # 4b. Dots and colons in project name, repeat start idempotent
    cat >"$TMUXINATOR_CONFIG/dotted.yml" <<EOF
name: my.proj:1
attach: false
socket_name: $SOCK
windows:
  - main: echo ok
EOF
    "$MUX" start dotted
    "$MUX" start dotted
    SESSIONS=$(tmux -L "$SOCK" list-sessions -F '#S')
    assert_eq "my_proj_1" "$SESSIONS" "dots/colons sanitised and second start succeeds"
    "$MUX" stop dotted

    # 4c. Duplicate window names, dotted window names, numeric window names, nameless window
    cat >"$TMUXINATOR_CONFIG/win_edge.yml" <<EOF
name: win_edge
attach: false
socket_name: $SOCK
windows:
  - ~: echo nameless
  - shell: echo one
  - shell: echo two
  - api.v2: echo dotted
  - 0: echo numeric
EOF
    if "$MUX" start win_edge; then
        WIN_COUNT=$(tmux -L "$SOCK" list-windows -t =win_edge | wc -l | tr -d ' ')
        assert_eq "5" "$WIN_COUNT" "nameless, duplicate, dotted, and numeric windows all created"
    else
        fail "win_edge start failed"
    fi
    "$MUX" stop win_edge

    # 4c2. list --active matches YAML name: even when filename differs
    cat >"$TMUXINATOR_CONFIG/alias_cfg.yml" <<EOF
name: actual_session
attach: false
socket_name: $SOCK
windows:
  - main: echo active
EOF
    "$MUX" start alias_cfg
    if "$MUX" list --active | grep -Fxq "alias_cfg"; then
        pass "list --active matches project by YAML name and custom socket"
    else
        fail "list --active did not report alias_cfg"
    fi
    "$MUX" stop alias_cfg

    # 4d. Out-of-range focused_pane/startup_pane + default pane 0 focus + hooks with unset var & project root
    PROJ_ROOT="$TMPDIR_BASE/my proj root"
    SUB_DIR="$PROJ_ROOT/sub"
    mkdir -p "$SUB_DIR"
    HOOK_LOG="$TMPDIR_BASE/hook.log"
    cat >"$TMUXINATOR_CONFIG/hooks_roots.yml" <<EOF
name: hooks_roots
root: $PROJ_ROOT
attach: false
socket_name: $SOCK
startup_pane: 99
enable_pane_titles: true
on_project_start:
  - echo "\$MUX_DEFINITELY_UNSET_VAR"
  - pwd > "$HOOK_LOG"
windows:
  - main:
      root: sub
      focused_pane: 99
      panes:
        - editor: echo p0
        - shell: echo p1
EOF
    if "$MUX" start hooks_roots; then
        pass "hooks with unset var and out-of-range pane indices succeed"
        HOOK_PWD=$(cat "$HOOK_LOG")
        assert_eq "$PROJ_ROOT" "$HOOK_PWD" "hooks run in project root"

        ACTIVE_PANE=$(tmux -L "$SOCK" display-message -p -t =hooks_roots:0 '#{pane_index}')
        assert_eq "0" "$ACTIVE_PANE" "default/fallback focus lands on pane 0"

        PANE_PATH=$(tmux -L "$SOCK" display-message -p -t =hooks_roots:0.0 '#{pane_current_path}')
        assert_eq "$SUB_DIR" "$PANE_PATH" "relative window root resolves against project root"

        GLOBAL_BORDER=$(tmux -L "$SOCK" show-option -gv pane-border-status 2>/dev/null || echo off)
        WIN_BORDER=$(tmux -L "$SOCK" show-window-option -t =hooks_roots:0 -v pane-border-status)
        WIN_FMT=$(tmux -L "$SOCK" show-window-option -t =hooks_roots:0 -v pane-border-format)
        assert_eq "off" "$GLOBAL_BORDER" "pane-border-status is not mutated globally"
        assert_eq "top" "$WIN_BORDER" "pane-border-status set on window"
        assert_eq '#{pane_index}: #{pane_title}' "$WIN_FMT" "default pane-border-format applied"
    else
        fail "hooks_roots start failed"
    fi
    "$MUX" stop hooks_roots

    # 4e. --append targets the CURRENT tmux session, never the config's own
    #     session, and never attaches (upstream template: attach? && !append?)
    cat >"$TMUXINATOR_CONFIG/append_cfg.yml" <<EOF
name: app_target
attach: false
socket_name: $SOCK
windows:
  - added: sleep 60
EOF
    tmux_new_session -d -s host_sess 'sleep 300'
    RC_FILE="$TMPDIR_BASE/append_rc"
    rm -f "$RC_FILE"
    tmux -L "$SOCK" new-window -d -t =host_sess \
        "sh -c '$MUX start --append append_cfg; echo \$? >$RC_FILE'"
    for _ in $(seq 1 100); do
        if [ -f "$RC_FILE" ]; then break; fi
        sleep 0.1
    done
    if [ -f "$RC_FILE" ] && [ "$(cat "$RC_FILE")" = "0" ]; then
        pass "append from inside session exits 0"
    else
        fail "append from inside session failed (rc=$(cat "$RC_FILE" 2>/dev/null))"
    fi
    if tmux -L "$SOCK" list-windows -t =host_sess -F '#W' | grep -Fxq added; then
        pass "append modifies the current session (host_sess)"
    else
        fail "append did not add window to host_sess"
    fi
    if tmux -L "$SOCK" has-session -t =app_target 2>/dev/null; then
        fail "append created the config's own session instead of using the current one"
    else
        pass "append never creates the config's own session"
    fi
    tmux -L "$SOCK" kill-session -t =host_sess
    if "$MUX" debug -a -p "$TMUXINATOR_CONFIG/append_cfg.yml" | grep -q "attach-session\|switch-client"; then
        fail "append script must not attach or switch"
    else
        pass "append script has no attach/switch"
    fi
    if ERR=$("$MUX" start --append append_cfg 2>&1 >/dev/null); then
        fail "append outside tmux should exit non-zero"
    else
        case "$ERR" in
        *"Cannot append to a session that does not exist"*)
            pass "append outside tmux errors with upstream message"
            ;;
        *) fail "append outside tmux gave unexpected error: [$ERR]" ;;
        esac
    fi

    # 4h. Non-fatal tmux failures (invalid layout) exit non-zero but still
    #     build every window
    cat >"$TMUXINATOR_CONFIG/bad_layout.yml" <<EOF
name: bad_layout
attach: false
socket_name: $SOCK
windows:
  - one: echo x
  - two:
      layout: definitely-not-a-layout
      panes:
        - echo a
        - echo b
EOF
    if ERR=$("$MUX" start bad_layout 2>&1 >/dev/null); then
        fail "invalid layout should exit non-zero"
    else
        case "$ERR" in
        *"tmux command(s) failed"*) pass "invalid layout exits non-zero with mux error" ;;
        *) fail "invalid layout exit missing mux error: [$ERR]" ;;
        esac
    fi
    WIN_COUNT=$(tmux -L "$SOCK" list-windows -t =bad_layout | wc -l | tr -d ' ')
    assert_eq "2" "$WIN_COUNT" "all windows created despite invalid layout"
    PANE_COUNT=$(tmux -L "$SOCK" list-panes -t =bad_layout:1 | wc -l | tr -d ' ')
    assert_eq "2" "$PANE_COUNT" "panes created despite invalid layout"
    "$MUX" stop bad_layout

    # 4i. Relative project root resolves against the invocation directory
    #     (like Ruby File.expand_path), not against the script's cd
    mkdir -p "$TMPDIR_BASE/relproj/app"
    cat >"$TMUXINATOR_CONFIG/relroot.yml" <<EOF
name: relroot
attach: false
socket_name: $SOCK
root: relproj/app
windows:
  - one: sleep 60
EOF
    (cd "$TMPDIR_BASE" && "$MUX" start relroot)
    PANE_CWD=$(tmux -L "$SOCK" display-message -p -t =relroot:0.0 '#{pane_current_path}')
    assert_eq "$TMPDIR_BASE/relproj/app" "$PANE_CWD" "relative root resolves against invocation cwd"
    "$MUX" stop relroot

    # 4j. Explicitly invalid startup_pane (e.g. -1) focuses pane 0, not
    #     focused_pane
    cat >"$TMUXINATOR_CONFIG/badpane.yml" <<EOF
name: badpane
attach: false
socket_name: $SOCK
startup_pane: -1
windows:
  - editor:
      focused_pane: 1
      panes:
        - sleep 60
        - sleep 60
EOF
    if "$MUX" start badpane; then
        ACTIVE_PANE=$(tmux -L "$SOCK" display-message -p -t =badpane:0 '#{pane_index}')
        assert_eq "0" "$ACTIVE_PANE" "startup_pane: -1 focuses pane 0, not focused_pane"
    else
        fail "badpane start failed"
    fi
    "$MUX" stop badpane

    # 4f. stop-all only stops mux-managed sessions and runs on_project_stop
    tmux_new_session -d -s unrelated
    STOP_MARKER="$TMPDIR_BASE/stopped"
    cat >"$TMUXINATOR_CONFIG/managed.yml" <<EOF
name: managed
attach: false
socket_name: $SOCK
on_project_stop: touch "$STOP_MARKER"
windows:
  - main: echo hi
EOF
    "$MUX" start managed
    printf 'y\n' | "$MUX" stop-all >/dev/null
    if [ -f "$STOP_MARKER" ]; then
        pass "stop-all runs on_project_stop"
    else
        fail "stop-all did not run on_project_stop"
    fi
    SESSIONS=$(tmux -L "$SOCK" list-sessions -F '#S')
    assert_eq "unrelated" "$SESSIONS" "stop-all leaves non-mux sessions untouched"
    tmux -L "$SOCK" kill-session -t =unrelated

    # 4g. Non-zero base-index and pane-base-index + tilde root with spaces
    TMUX_CONF="$TMPDIR_BASE/base_idx.conf"
    cat >"$TMUX_CONF" <<EOF
set -g base-index 1
setw -g pane-base-index 1
EOF
    SPACE_DIR="$HOME/.cache/mux itest $$"
    mkdir -p "$SPACE_DIR"
    cat >"$TMUXINATOR_CONFIG/base_idx.yml" <<EOF
name: base_idx
root: ~/.cache/mux itest $$
attach: false
socket_name: $SOCK
tmux_options: -f $TMUX_CONF
windows:
  - first:
      panes:
        - echo p1
        - echo p2
  - second: echo w2
EOF
    if "$MUX" start base_idx; then
        FIRST_WIN_IDX=$(tmux -L "$SOCK" list-windows -t =base_idx -F '#{window_index}' | head -n 1)
        PANE_DIR=$(tmux -L "$SOCK" display-message -p -t =base_idx:1.1 '#{pane_current_path}')
        ACTIVE_PANE=$(tmux -L "$SOCK" display-message -p -t =base_idx:1 '#{pane_index}')
        assert_eq "1" "$FIRST_WIN_IDX" "non-zero base-index respected"
        assert_eq "1" "$ACTIVE_PANE" "non-zero pane-base-index focuses first pane"
        assert_eq "$SPACE_DIR" "$PANE_DIR" "tilde root with spaces expands properly"
    else
        fail "base_idx start failed"
    fi
    "$MUX" stop base_idx
    rmdir "$SPACE_DIR" 2>/dev/null || true
fi

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] || exit 1
