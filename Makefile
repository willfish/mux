.PHONY: build test clean setup demo

# Build the project
build:
	@if [ ! -d build ]; then nix develop --command meson setup build; fi
	nix develop --command meson compile -C build

# Run tests
test: build
	nix develop --command meson test -C build

# Setup build directory
setup:
	nix develop --command meson setup build --reconfigure

# Clean build artifacts
clean:
	rm -rf build

# Record demo GIF (uses xdg-desktop-portal for screen capture)
demo: build
	nix-shell -p gpu-screen-recorder ffmpeg --run "bash demo/record-demo.sh"
