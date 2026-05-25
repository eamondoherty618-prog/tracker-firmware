$ErrorActionPreference = "Stop"

Write-Output "=== Install WSL Platform ==="
wsl --install --no-distribution

Write-Output "=== WSL Status After Install Attempt ==="
try {
    wsl --status
} catch {
    Write-Output "wsl --status failed: $($_.Exception.Message)"
}
