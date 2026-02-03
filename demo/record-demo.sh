#!/usr/bin/env bash
# Record a video demo of mux and convert to GIF
# Requires: gpu-screen-recorder, ffmpeg
#
# Usage: nix-shell -p gpu-screen-recorder ffmpeg --run "bash demo/record-demo.sh"

cd "$(dirname "$0")/.."

VIDEO_FILE="/tmp/mux-demo.mp4"
GIF_FILE="demo.gif"

echo "=== mux Demo Recorder ==="
echo ""
echo "This will record mux starting a tmux session with 3 panes:"
echo "  - btop/htop system monitor"
echo "  - vim editing a file"
echo "  - live log viewer"
echo ""
echo "Steps:"
echo "  1. Press Enter - screen will clear"
echo "  2. Portal dialog appears - select your terminal window"
echo "  3. Press Enter again to start the demo"
echo ""
read -p "Press Enter to continue..."

# Cleanup any existing session and old video
tmux kill-session -t demo 2>/dev/null || true
rm -f "$VIDEO_FILE"

# Clear screen BEFORE starting recorder
clear

# Start recording with portal (works on any Wayland compositor)
# The portal dialog will appear over the cleared screen
gpu-screen-recorder -w portal -f 24 -o "$VIDEO_FILE" >/dev/null 2>&1 &
RECORDER_PID=$!

# Wait for user to complete portal dialog
# Don't print anything - the recorder is already capturing
read -s -p ""

# Check if recorder is running
if ! kill -0 $RECORDER_PID 2>/dev/null; then
    echo "gpu-screen-recorder failed to start or was cancelled"
    exit 1
fi

# Type effect
type_cmd() {
    local cmd="$1"
    for (( i=0; i<${#cmd}; i++ )); do
        printf '%s' "${cmd:$i:1}"
        sleep 0.035
    done
    sleep 0.25
    printf '\n'
}

# Show config briefly
type_cmd "cat demo/demo.yml"
cat demo/demo.yml
sleep 1.5

clear

# Auto-detach after showing the panes
(
    sleep 4
    tmux detach-client -s demo 2>/dev/null
) &

type_cmd "mux start -p demo/demo.yml"
./build/mux start -p demo/demo.yml

sleep 0.5

# Stop recording (don't print - it gets recorded)
kill -INT $RECORDER_PID 2>/dev/null
sleep 2
wait $RECORDER_PID 2>/dev/null || true

# Cleanup tmux
tmux kill-session -t demo 2>/dev/null || true

# Check video file
if [ ! -f "$VIDEO_FILE" ]; then
    echo "Error: Video file was not created"
    exit 1
fi

echo "Video saved: $(ls -lh "$VIDEO_FILE")"

# Convert to GIF
echo "Converting to GIF..."
ffmpeg -y -i "$VIDEO_FILE" \
    -vf "fps=12,scale=800:-1:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=128[p];[s1][p]paletteuse=dither=bayer" \
    -loop 0 \
    "$GIF_FILE"

echo ""
ls -lh "$GIF_FILE"
rm -f "$VIDEO_FILE"
echo "Saved to $GIF_FILE"
