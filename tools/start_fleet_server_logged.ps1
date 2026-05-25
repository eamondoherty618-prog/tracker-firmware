$serverRoot = "C:\ops\fleet-tracker\server"
$serverScript = Join-Path $serverRoot "fleet_server.py"
$stdoutLog = Join-Path $serverRoot "fleet_server_stdout.log"
$stderrLog = Join-Path $serverRoot "fleet_server_stderr.log"

$existing = Get-CimInstance Win32_Process | Where-Object { $_.Name -match "python|py" -and $_.CommandLine -like "*fleet_server.py*" }
if ($existing) {
    $existing | ForEach-Object {
        try {
            Stop-Process -Id $_.ProcessId -Force -ErrorAction Stop
        } catch {
            Write-Output "STOP-WARN: $($_.Exception.Message)"
        }
    }
}

Remove-Item $stdoutLog, $stderrLog -Force -ErrorAction SilentlyContinue

$process = Start-Process -FilePath py `
    -ArgumentList "-3", $serverScript, "--host", "127.0.0.1", "--port", "8081" `
    -WorkingDirectory $serverRoot `
    -RedirectStandardOutput $stdoutLog `
    -RedirectStandardError $stderrLog `
    -WindowStyle Hidden `
    -PassThru

Start-Sleep -Seconds 3

Write-Output "PID=$($process.Id)"
Write-Output "TCP=$((Test-NetConnection 127.0.0.1 -Port 8081).TcpTestSucceeded)"

if (Test-Path $stdoutLog) {
    Write-Output "--- STDOUT ---"
    Get-Content $stdoutLog -ErrorAction SilentlyContinue
}

if (Test-Path $stderrLog) {
    Write-Output "--- STDERR ---"
    Get-Content $stderrLog -ErrorAction SilentlyContinue
}
