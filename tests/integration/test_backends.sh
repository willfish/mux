#!/usr/bin/env bash
set -euo pipefail

MUX="${MUX:-./build/mux}"
PASS=0
FAIL=0

TMPDIR_BASE=$(mktemp -d)
trap 'rm -rf "$TMPDIR_BASE"' EXIT

pass() {
    echo "  PASS: $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "  FAIL: $1"
    FAIL=$((FAIL + 1))
}

assert_contains() {
    local file="$1"
    local needle="$2"
    local label="$3"
    if grep -Fq -- "$needle" "$file"; then
        pass "$label"
    else
        echo "    expected to find: [$needle] in $file"
        fail "$label"
    fi
}

assert_not_contains() {
    local file="$1"
    local needle="$2"
    local label="$3"
    if grep -Fq -- "$needle" "$file"; then
        echo "    unexpectedly found: [$needle] in $file"
        fail "$label"
    else
        pass "$label"
    fi
}

export TMUXINATOR_CONFIG="$TMPDIR_BASE/configs"
mkdir -p "$TMUXINATOR_CONFIG" "$TMPDIR_BASE/bin" "$TMPDIR_BASE/my root/sub"

# ==============================================================================
# 1. Herdr TUI backend end-to-end execution test (stateful mock CLI)
# ==============================================================================
echo "# herdr backend integration"

HERDR_STATE="$TMPDIR_BASE/herdr_state.json"
HERDR_LOG="$TMPDIR_BASE/herdr.log"
HERDR_SERVER_UP="$TMPDIR_BASE/herdr_server_up"
HERDR_MOCK="$TMPDIR_BASE/bin/mock-herdr"

cat >"$HERDR_MOCK" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
STATE="${MOCK_HERDR_STATE:?}"
LOG="${MOCK_HERDR_LOG:?}"
UP="${MOCK_HERDR_UP:?}"

printf '%s\n' "$*" >>"$LOG"

case "${1:-}" in
    status)
        if [ -f "$UP" ]; then
            echo "status: running"
        else
            echo "status: not running"
        fi
        ;;
    server)
        touch "$UP"
        ;;
    workspace)
        sub="$2"; shift 2
        python3 - "$STATE" "$sub" "$@" <<'PY'
import json, os, sys
state_path, sub, args = sys.argv[1], sys.argv[2], sys.argv[3:]
state = {"seq": 0, "workspaces": []}
if os.path.exists(state_path):
    with open(state_path) as f:
        state = json.load(f)
if sub == "list":
    print(json.dumps({"result": {"workspaces": state["workspaces"]}}))
elif sub == "create":
    label = ""
    if "--label" in args:
        label = args[args.index("--label") + 1]
    state["seq"] += 1
    n = state["seq"]
    ws = {"workspace_id": f"ws_{n}", "label": label}
    state["workspaces"].append(ws)
    with open(state_path, "w") as f:
        json.dump(state, f)
    print(json.dumps({"result": {"workspace_id": f"ws_{n}", "tab_id": f"tab_{n}_0", "pane_id": f"pane_{n}_0"}}))
elif sub == "focus":
    print(json.dumps({"ok": True}))
elif sub == "close":
    wid = args[0]
    state["workspaces"] = [w for w in state["workspaces"] if w["workspace_id"] != wid]
    with open(state_path, "w") as f:
        json.dump(state, f)
    print(json.dumps({"ok": True}))
PY
        ;;
    tab)
        sub="$2"; shift 2
        if [ "$sub" = "create" ]; then
            python3 - "$STATE" <<'PY'
import json, os, sys
state_path = sys.argv[1]
with open(state_path) as f:
    state = json.load(f)
state["seq"] += 1
n = state["seq"]
with open(state_path, "w") as f:
    json.dump(state, f)
print(json.dumps({"result": {"tab_id": f"tab_{n}", "pane_id": f"pane_{n}_0"}}))
PY
        else
            echo '{"ok":true}'
        fi
        ;;
    pane)
        sub="$2"; shift 2
        if [ "$sub" = "split" ]; then
            python3 - "$STATE" <<'PY'
import json, os, sys
state_path = sys.argv[1]
with open(state_path) as f:
    state = json.load(f)
state["seq"] += 1
n = state["seq"]
with open(state_path, "w") as f:
    json.dump(state, f)
print(json.dumps({"result": {"pane_id": f"pane_split_{n}"}}))
PY
        else
            echo '{"ok":true}'
        fi
        ;;
    session)
        echo '{"ok":true}'
        ;;
esac
EOF
chmod +x "$HERDR_MOCK"

HERDR_HOOKS="$TMPDIR_BASE/herdr_hooks.log"
cat >"$TMUXINATOR_CONFIG/herdr_proj.yml" <<EOF
name: herdr_proj
root: $TMPDIR_BASE/my root
attach: false
pre_window: echo PRE
startup_window: 1
startup_pane: 99
on_project_start:
  - echo "start:\$UNSET_HERDR_VAR:$(pwd)" >> "$HERDR_HOOKS"
  - pwd >> "$HERDR_HOOKS"
on_project_first_start: echo first >> "$HERDR_HOOKS"
on_project_restart: echo restart >> "$HERDR_HOOKS"
on_project_exit: echo exit >> "$HERDR_HOOKS"
on_project_stop: echo stop >> "$HERDR_HOOKS"
windows:
  - editor:
      layout: main-vertical
      focused_pane: 99
      panes:
        - Editor: vim
        - Shell: bash
        - Logs: tail -f app.log
  - api:
      root: sub
      panes:
        - npm start
EOF

export MUX_HERDR_COMMAND="$HERDR_MOCK"
export MOCK_HERDR_STATE="$HERDR_STATE"
export MOCK_HERDR_LOG="$HERDR_LOG"
export MOCK_HERDR_UP="$HERDR_SERVER_UP"

# First start auto-starts server, creates workspace/tabs/panes, runs hooks in root
"$MUX" start --backend herdr herdr_proj
assert_contains "$HERDR_LOG" "server" "herdr auto-starts server when stopped"
assert_contains "$HERDR_LOG" "workspace create --focus --cwd $TMPDIR_BASE/my root --label herdr_proj" "herdr creates workspace with root and label"
assert_contains "$HERDR_LOG" "tab create --workspace ws_1 --no-focus --cwd $TMPDIR_BASE/my root/sub --label api" "herdr joins relative window root"
assert_contains "$HERDR_LOG" "pane split pane_1_0 --direction right --ratio 0.333333 --no-focus" "herdr splits panes with balanced ratio and keeps focus on pane 0"
assert_contains "$HERDR_LOG" "pane rename pane_1_0 Editor" "herdr renames named panes"
assert_not_contains "$HERDR_LOG" "pane focus" "herdr does not invoke invalid positional pane focus"
assert_contains "$HERDR_LOG" "tab focus tab_4" "herdr supports numeric startup_window index"
assert_not_contains "$HERDR_LOG" "session attach" "herdr respects attach: false"
assert_contains "$HERDR_HOOKS" "$TMPDIR_BASE/my root" "herdr hooks run in project root"
assert_contains "$HERDR_HOOKS" "first" "herdr runs on_project_first_start"
assert_contains "$HERDR_HOOKS" "exit" "herdr runs on_project_exit"

# Second start reuses existing workspace and runs on_project_restart
"$MUX" start --backend herdr herdr_proj
assert_contains "$HERDR_HOOKS" "restart" "herdr runs on_project_restart on repeat start"

# Stop closes matching workspace and runs on_project_stop
"$MUX" stop --backend herdr herdr_proj
assert_contains "$HERDR_LOG" "workspace close ws_1" "herdr stop closes workspace"
assert_contains "$HERDR_HOOKS" "stop" "herdr stop runs on_project_stop"

# ==============================================================================
# 2. Herdr desktop app (herdr-app + hc) backend end-to-end execution test
# ==============================================================================
echo "# herdr-app backend integration"

APP_STATE="$TMPDIR_BASE/app_state.json"
APP_LOG="$TMPDIR_BASE/app.log"
HC_LOG="$TMPDIR_BASE/hc.log"
APP_MOCK="$TMPDIR_BASE/bin/mock-herdr-app"
HC_MOCK="$TMPDIR_BASE/bin/mock-hc"

cat >"$APP_MOCK" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
STATE="${MOCK_APP_STATE:?}"
LOG="${MOCK_APP_LOG:?}"

printf '%s\n' "$*" >>"$LOG"

python3 - "$STATE" "$@" <<'PY'
import json, os, sys
state_path = sys.argv[1]
args = sys.argv[2:]
# Seed a remote space with the same name to verify remote spaces are ignored
state = {
    "seq": 0,
    "spaces": [
        {
            "id": "sp_remote",
            "name": "app_proj",
            "machine": "remote-box",
            "tabs": [],
        }
    ],
}
if os.path.exists(state_path):
    with open(state_path) as f:
        state = json.load(f)

if args[0] == "ls":
    print(json.dumps({"spaces": state["spaces"]}))
elif args[0] == "do":
    action = args[1]
    rest = args[2:]
    def opt(flag, default=""):
        return rest[rest.index(flag) + 1] if flag in rest else default
    if action == "space.new":
        state["seq"] += 1
        n = state["seq"]
        sp_id, tb_id, pn_id, tm_id = f"sp_{n}", f"tb_{n}", f"pn_{n}", f"tm_{n}"
        state["spaces"].append({
            "id": sp_id,
            "name": opt("--name"),
            "machine": opt("--machine"),
            "tabs": [{"id": tb_id, "name": opt("--tab"), "terminals": [{"pane": pn_id, "terminal": tm_id}]}],
        })
        with open(state_path, "w") as f:
            json.dump(state, f)
        print(json.dumps({"space": sp_id, "tab": tb_id, "terminal": tm_id}))
    elif action == "tab.new":
        state["seq"] += 1
        n = state["seq"]
        sp_id = opt("--space")
        tb_id, pn_id, tm_id = f"tb_{n}", f"pn_{n}", f"tm_{n}"
        for sp in state["spaces"]:
            if sp["id"] == sp_id:
                sp["tabs"].append({"id": tb_id, "name": opt("--name"), "terminals": [{"pane": pn_id, "terminal": tm_id}]})
        with open(state_path, "w") as f:
            json.dump(state, f)
        print(json.dumps({"tab": tb_id, "terminal": tm_id}))
    elif action == "pane.split":
        state["seq"] += 1
        n = state["seq"]
        pn_id, tm_id = f"pn_{n}", f"tm_{n}"
        with open(state_path, "w") as f:
            json.dump(state, f)
        print(json.dumps({"pane": pn_id, "terminal": tm_id}))
    elif action == "space.close":
        sp_id = opt("--id")
        state["spaces"] = [s for s in state["spaces"] if s["id"] != sp_id]
        with open(state_path, "w") as f:
            json.dump(state, f)
        print(json.dumps({"ok": True}))
    else:
        print(json.dumps({"ok": True}))
PY
EOF
chmod +x "$APP_MOCK"

cat >"$HC_MOCK" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
LOG="${MOCK_HC_LOG:?}"
printf 'SEND [%s] => %s\n' "$3" "$4" >>"$LOG"
EOF
chmod +x "$HC_MOCK"

APP_HOOKS="$TMPDIR_BASE/app_hooks.log"
cat >"$TMUXINATOR_CONFIG/app_proj.yml" <<EOF
name: app_proj
root: $TMPDIR_BASE/my root
pre_window: echo READY
startup_window: api
startup_pane: 0
on_project_start: pwd >> "$APP_HOOKS"
on_project_first_start: echo first >> "$APP_HOOKS"
on_project_restart: echo restart >> "$APP_HOOKS"
on_project_exit: echo exit >> "$APP_HOOKS"
on_project_stop: echo stop >> "$APP_HOOKS"
windows:
  - editor:
      layout: main-vertical
      panes:
        - vim
        - guard
  - api:
      root: sub
      panes:
        - echo "hello \$HOME"
EOF

export MUX_HERDR_APP_COMMAND="$APP_MOCK"
export MUX_HC_COMMAND="$HC_MOCK"
export MOCK_APP_STATE="$APP_STATE"
export MOCK_APP_LOG="$APP_LOG"
export MOCK_HC_LOG="$HC_LOG"

# First start creates local space despite remote space with same name
MUX_BACKEND=herdr-app "$MUX" start app_proj
assert_contains "$APP_LOG" "do space.new --machine  --name app_proj --tab editor --folder $TMPDIR_BASE/my root" "herdr-app creates local space ignoring remote space"
assert_contains "$APP_LOG" "do tab.new --space sp_1 --name api --folder $TMPDIR_BASE/my root/sub" "herdr-app resolves relative window root"
assert_contains "$APP_LOG" "do pane.split --pane pn_1 --direction right" "herdr-app splits pane before sending commands"
assert_contains "$APP_LOG" "do tab.select --id tb_3" "herdr-app selects startup_window tab"
assert_contains "$HC_LOG" "SEND [tm_1] => cd --" "herdr-app sends pure-bash quoted cd command"
assert_contains "$HC_LOG" "echo READY" "herdr-app includes pre_window command"
# shellcheck disable=SC2016 # the literal, unexpanded $HOME is the point
assert_contains "$HC_LOG" 'echo "hello $HOME"' "herdr-app preserves shell variables in pane commands"
assert_contains "$APP_HOOKS" "$TMPDIR_BASE/my root" "herdr-app hooks run in project root"
assert_contains "$APP_HOOKS" "first" "herdr-app runs on_project_first_start"

# Repeat start selects existing local space without re-sending pane commands
HC_LINES_BEFORE=$(wc -l <"$HC_LOG")
MUX_BACKEND=herdr-app "$MUX" start app_proj
HC_LINES_AFTER=$(wc -l <"$HC_LOG")
if [ "$HC_LINES_BEFORE" = "$HC_LINES_AFTER" ]; then
    pass "herdr-app repeat start does not re-send pane commands"
else
    fail "herdr-app repeat start re-sent commands ($HC_LINES_BEFORE -> $HC_LINES_AFTER)"
fi
assert_contains "$APP_LOG" "do space.select --id sp_1" "herdr-app repeat start selects local space"
assert_contains "$APP_HOOKS" "restart" "herdr-app repeat start runs on_project_restart"

# Stop closes local space and leaves remote space intact
MUX_BACKEND=herdr-app "$MUX" stop app_proj
assert_contains "$APP_LOG" "do space.close --id sp_1 --confirmed true" "herdr-app stop closes local space"
assert_not_contains "$APP_LOG" "sp_remote" "herdr-app stop leaves remote space untouched"
assert_contains "$APP_HOOKS" "stop" "herdr-app stop runs on_project_stop"

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] || exit 1
