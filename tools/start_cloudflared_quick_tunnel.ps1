$cloudflared = "C:\Program Files (x86)\cloudflared\cloudflared.exe"
$appRoot = "C:\ops\fleet-tracker"
$stdoutLog = Join-Path $appRoot "cloudflared_stdout.log"
$stderrLog = Join-Path $appRoot "cloudflared_stderr.log"

$existing = Get-CimInstance Win32_Process | Where-Object { $_.Name -match "cloudflared" -and $_.CommandLine -like "*trycloudflare*" }
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

$process = Start-Process -FilePath $cloudflared `
    -ArgumentList "tunnel", "--url", "http://127.0.0.1:8081", "--no-autoupdate" `
    -WorkingDirectory $appRoot `
    -RedirectStandardOutput $stdoutLog `
    -RedirectStandardError $stderrLog `
    -WindowStyle Hidden `
    -PassThru

$url = $null
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Seconds 1
    $content = ""
    if (Test-Path $stdoutLog) { $content += (Get-Content $stdoutLog -Raw -ErrorAction SilentlyContinue) }
    if (Test-Path $stderrLog) { $content += "`n" + (Get-Content $stderrLog -Raw -ErrorAction SilentlyContinue) }
    $match = [regex]::Match($content, 'https://[-a-z0-9]+\.trycloudflare\.com')
    if ($match.Success) {
        $url = $match.Value
        break
    }
}

Write-Output "PID=$($process.Id)"
Write-Output "URL=$url"
if (Test-Path $stderrLog) {
    Write-Output "--- STDERR ---"
    Get-Content $stderrLog -ErrorAction SilentlyContinue
}
