#!/usr/bin/env bash
set -euo pipefail

MUX="${MUX:-./builddir/mux}"
PASS=0
FAIL=0

pass() { echo "  PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "  FAIL: $1"; FAIL=$((FAIL + 1)); }

echo "Running integration tests..."
echo

# Test: version output
echo "# mux version"
if $MUX version | grep -q "mux 0.1.0"; then
    pass "version output matches"
else
    fail "version output"
fi

# Test: help output
echo "# mux help"
if $MUX help | grep -q "Usage:"; then
    pass "help output contains Usage"
else
    fail "help output"
fi

# Test: list shows projects
echo "# mux list"
OUTPUT=$($MUX list)
if echo "$OUTPUT" | grep -q "work"; then
    pass "list shows work project"
else
    fail "list shows work project"
fi

# Test: debug generates valid bash
echo "# mux debug work"
SCRIPT=$($MUX debug work)
if echo "$SCRIPT" | head -1 | grep -q "#!/usr/bin/env bash"; then
    pass "debug output starts with shebang"
else
    fail "debug output shebang"
fi
if echo "$SCRIPT" | grep -q "new-session"; then
    pass "debug output contains new-session"
else
    fail "debug output contains new-session"
fi
if echo "$SCRIPT" | grep -q "has-session"; then
    pass "debug output contains has-session check"
else
    fail "debug output contains has-session"
fi

# Test: debug with --project-config flag
echo "# mux debug --project-config"
SCRIPT=$($MUX debug -p tests/fixtures/fun.yml)
if echo "$SCRIPT" | grep -q "fun"; then
    pass "debug with --project-config works"
else
    fail "debug with --project-config"
fi

# Test: debug with --name override
echo "# mux debug --name override"
SCRIPT=$($MUX debug --name custom work)
if echo "$SCRIPT" | grep -q "custom"; then
    pass "debug with --name override works"
else
    fail "debug with --name override"
fi

# Test: implicit start (project name as command)
echo "# mux <project> implicit start"
if $MUX debug work >/dev/null 2>&1; then
    pass "implicit project name works"
else
    fail "implicit project name"
fi

# Test: doctor
echo "# mux doctor"
if $MUX doctor | grep -q "tmux"; then
    pass "doctor checks tmux"
else
    fail "doctor checks tmux"
fi

# Test: completions
echo "# mux completions"
if $MUX completions bash | grep -q "complete"; then
    pass "bash completions generated"
else
    fail "bash completions"
fi
if $MUX completions zsh | grep -q "compdef"; then
    pass "zsh completions generated"
else
    fail "zsh completions"
fi
if $MUX completions fish | grep -q "complete -c mux"; then
    pass "fish completions generated"
else
    fail "fish completions"
fi

# Test: unknown project
echo "# mux debug nonexistent"
if $MUX debug nonexistent_xyz 2>/dev/null; then
    fail "should error on unknown project"
else
    pass "errors on unknown project"
fi

echo
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ] || exit 1
