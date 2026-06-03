# ---------------------------------------------------
# Setup script for test-bench-gui (Windows PowerShell)
# Run from the project folder:
#   .\setup_repo.ps1
# ---------------------------------------------------

$ErrorActionPreference = "Stop"

Write-Host "=== Initializing git repository ===" -ForegroundColor Cyan
git init
git branch -M main

Write-Host "=== Adding files ===" -ForegroundColor Cyan
git add .gitignore protocol.py tcp_client.py main_window.py requirements.txt

Write-Host "=== Creating initial commit ===" -ForegroundColor Cyan
git commit -m "feat: initial project structure with diagnostic tab

- protocol.py: binary TCP protocol (STX/CMD/LEN/PAYLOAD/CRC16/ETX)
- tcp_client.py: QThread-based TCP client with Qt signals
- main_window.py: PyQt6 GUI with diagnostics tab (ping, LED, echo, status)
- Cross-platform support (Windows + macOS)"

Write-Host "=== Adding remote origin ===" -ForegroundColor Cyan
git remote add origin git@github.com:wojtekdaw/test-bench-gui.git

Write-Host "=== Pushing to GitHub ===" -ForegroundColor Cyan
git push -u origin main

Write-Host ""
Write-Host "Done! Repository: https://github.com/wojtekdaw/test-bench-gui" -ForegroundColor Green
