#!/usr/bin/env bash
# Demo script for mux - generates the README demo GIF
# Run with: asciinema rec demo/demo.cast --command 'bash demo/demo-script.sh' --cols 100 --rows 30

# Change to repo root (one level up from demo/)
cd "$(dirname "$0")/.."

type_cmd() {
  local cmd="$1"
  for (( i=0; i<${#cmd}; i++ )); do
    printf '%s' "${cmd:$i:1}"
    sleep 0.03
  done
  sleep 0.2
  printf '\n'
}

pause() { sleep "${1:-1.2}"; }

clear
pause 0.5

# Show help
type_cmd "mux help"
./build/mux help
pause 2

clear

# Show the demo config
type_cmd "cat demo/demo.yml"
cat demo/demo.yml
pause 2

clear

# Show the generated tmux script
type_cmd "mux debug -p demo/demo.yml"
./build/mux debug -p demo/demo.yml
pause 3
