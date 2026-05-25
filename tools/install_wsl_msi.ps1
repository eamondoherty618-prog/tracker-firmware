$ErrorActionPreference = "Stop"

$url = "https://github.com/microsoft/WSL/releases/download/2.6.3/wsl.2.6.3.0.x64.msi"
$msiPath = "C:\ops\fleet-tracker\wsl.2.6.3.0.x64.msi"
$logPath = "C:\ops\fleet-tracker\wsl-install.log"

Write-Output "=== Download WSL MSI ==="
Invoke-WebRequest -Uri $url -OutFile $msiPath

Write-Output "=== Install WSL MSI ==="
$process = Start-Process -FilePath "msiexec.exe" `
    -ArgumentList "/i", $msiPath, "/qn", "/norestart", "/L*v", $logPath `
    -Wait `
    -PassThru

Write-Output ("MSI_EXIT={0}" -f $process.ExitCode)

Write-Output "=== WSL Version ==="
try {
    wsl --version
} catch {
    Write-Output "wsl --version failed: $($_.Exception.Message)"
}

Write-Output "=== WSL Status ==="
try {
    wsl --status
} catch {
    Write-Output "wsl --status failed: $($_.Exception.Message)"
}

Write-Output "=== Last 40 lines of MSI log ==="
if (Test-Path $logPath) {
    Get-Content $logPath -Tail 40
}
