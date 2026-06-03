#!/bin/bash
# ---------------------------------------------------
# Setup script for test-bench-gui (macOS / Linux)
# Run from the project folder:
#   chmod +x setup_repo.sh && ./setup_repo.sh
# ---------------------------------------------------

set -e

echo "=== Initializing git repository ==="
git init
git branch -M main

echo "=== Adding files ==="
git add .gitignore protocol.py tcp_client.py main_window.py requirements.txt

echo "=== Creating initial commit ==="
git commit -m "feat: initial project structure with diagnostic tab

- protocol.py: binary TCP protocol (STX/CMD/LEN/PAYLOAD/CRC16/ETX)
- tcp_client.py: QThread-based TCP client with Qt signals
- main_window.py: PyQt6 GUI with diagnostics tab (ping, LED, echo, status)
- Cross-platform support (Windows + macOS)"

echo "=== Adding remote origin ==="
git remote add origin git@github.com:wojtekdaw/test-bench-gui.git

echo "=== Pushing to GitHub ==="
git push -u origin main

echo ""
echo "Done! Repository: https://github.com/wojtekdaw/test-bench-gui"
