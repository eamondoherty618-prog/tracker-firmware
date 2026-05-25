$ErrorActionPreference = "Stop"

Get-Process wsl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

Write-Output "=== Install Ubuntu via web download ==="
wsl --install Ubuntu --web-download --no-launch

Write-Output "=== Registered distros ==="
wsl -l -v
