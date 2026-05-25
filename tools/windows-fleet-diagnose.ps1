$ErrorActionPreference = "Continue"

$BaseUrl = "http://100.105.91.8:8765"
$ReportUrl = "http://100.105.91.8:8766/report"
$AppDir = "C:\ops\fleet-tracker"
$ServerScript = Join-Path $AppDir "server\fleet_server.py"
$Launcher = Join-Path $AppDir "run-fleet-tracker.ps1"
$TaskName = "FleetTrackerServer"

function Add-Line($lines, $text) {
    $lines.Add($text) | Out-Null
    Write-Host $text
}

function Get-TailscaleExe {
    $cmd = Get-Command tailscale.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @("$env:ProgramFiles\Tailscale\tailscale.exe", "${env:ProgramFiles(x86)}\Tailscale\tailscale.exe")
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return $candidate }
    }
    return $null
}

function Get-PythonExe {
    $python = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($python) { return $python.Source }
    $py = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($py) { return $py.Source }
    return $null
}

$lines = New-Object System.Collections.Generic.List[string]
Add-Line $lines "=== Fleet Tracker Windows Diagnose ==="
Add-Line $lines "Time: $(Get-Date -Format o)"
Add-Line $lines "Computer: $env:COMPUTERNAME"
Add-Line $lines "User: $env:USERNAME"

$tailscale = Get-TailscaleExe
Add-Line $lines "TailscaleExe: $tailscale"
if ($tailscale) {
    Add-Line $lines "--- tailscale status --self ---"
    Add-Line $lines ((& $tailscale status --self 2>&1) -join "`n")
    Add-Line $lines "--- tailscale funnel status ---"
    Add-Line $lines ((& $tailscale funnel status 2>&1) -join "`n")
}

$python = Get-PythonExe
Add-Line $lines "PythonExe: $python"
Add-Line $lines "AppDirExists: $(Test-Path $AppDir)"
Add-Line $lines "ServerScriptExists: $(Test-Path $ServerScript)"
Add-Line $lines "LauncherExists: $(Test-Path $Launcher)"

Add-Line $lines "--- Scheduled Task ---"
Add-Line $lines ((Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue | Format-List * | Out-String).Trim())

Add-Line $lines "--- fleet_server processes ---"
Add-Line $lines ((Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*fleet_server.py*" } | Select-Object ProcessId,CommandLine | Format-List | Out-String).Trim())

Add-Line $lines "--- Local Port 8081 ---"
Add-Line $lines ((Get-NetTCPConnection -LocalPort 8081 -ErrorAction SilentlyContinue | Select-Object LocalAddress,LocalPort,State,OwningProcess | Format-Table | Out-String).Trim())

Add-Line $lines "--- Local Health ---"
try {
    $health = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:8081/health" -TimeoutSec 5
    Add-Line $lines "HTTP $($health.StatusCode): $($health.Content)"
} catch {
    Add-Line $lines "FAILED: $($_.Exception.Message)"
}

if ($tailscale) {
    Add-Line $lines "--- Reapplying Funnel ---"
    Add-Line $lines ((& $tailscale set --hostname=ops-main-01 2>&1) -join "`n")
    Add-Line $lines ((& $tailscale funnel --bg --yes http://127.0.0.1:8081 2>&1) -join "`n")
    Add-Line $lines "--- tailscale funnel status after reapply ---"
    Add-Line $lines ((& $tailscale funnel status 2>&1) -join "`n")
}

$body = $lines -join "`n"
try {
    Invoke-WebRequest -UseBasicParsing -Method Post -Uri $ReportUrl -Body $body -ContentType "text/plain" -TimeoutSec 5 | Out-Null
    Write-Host "Report sent to Codex Mac."
} catch {
    Write-Host "Could not send report: $($_.Exception.Message)"
}

Write-Host ""
Write-Host "Done. If Codex does not respond with findings, send a photo of the output."
