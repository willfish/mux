#!/bin/sh
# Install script for mux
# Usage: curl -fsSL https://raw.githubusercontent.com/willfish/mux/master/install.sh | sh

set -e

REPO="willfish/mux"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"

# Detect OS
OS="$(uname -s)"
case "$OS" in
    Linux)  OS="linux" ;;
    Darwin) OS="darwin" ;;
    *)      echo "Unsupported OS: $OS"; exit 1 ;;
esac

# Detect architecture
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64|amd64)  ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64" ;;
    *)             echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac

TARGET="${OS}-${ARCH}"
echo "Detected platform: $TARGET"

# Get latest release tag
echo "Fetching latest release..."
LATEST=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')

if [ -z "$LATEST" ]; then
    echo "Error: Could not determine latest release"
    exit 1
fi

echo "Latest version: $LATEST"

# Download
URL="https://github.com/${REPO}/releases/download/${LATEST}/mux-${TARGET}.tar.gz"
echo "Downloading $URL..."

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

curl -fsSL "$URL" -o "$TMPDIR/mux.tar.gz"

# Extract
tar -xzf "$TMPDIR/mux.tar.gz" -C "$TMPDIR"

# Install
if [ -w "$INSTALL_DIR" ]; then
    mv "$TMPDIR/mux" "$INSTALL_DIR/mux"
else
    echo "Installing to $INSTALL_DIR (requires sudo)..."
    sudo mv "$TMPDIR/mux" "$INSTALL_DIR/mux"
fi

chmod +x "$INSTALL_DIR/mux"

echo ""
echo "mux $LATEST installed to $INSTALL_DIR/mux"
echo ""
echo "Run 'mux help' to get started"
