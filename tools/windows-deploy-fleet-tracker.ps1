$ErrorActionPreference = "Stop"

$BaseUrl = "http://100.105.91.8:8765"
$ReportUrl = "http://100.105.91.8:8766/report"
$AppDir = "C:\ops\fleet-tracker"
$ServerDir = Join-Path $AppDir "server"
$PublicDir = Join-Path $ServerDir "public"
$DataDir = Join-Path $AppDir "data"
$Launcher = Join-Path $AppDir "run-fleet-tracker.ps1"
$TaskName = "FleetTrackerServer"

$ReportLines = New-Object System.Collections.Generic.List[string]

function Log($message) {
    $ReportLines.Add($message) | Out-Null
    Write-Host $message
}

function Send-Report {
    $body = $ReportLines -join "`n"
    try {
        Invoke-WebRequest -UseBasicParsing -Method Post -Uri $ReportUrl -Body $body -ContentType "text/plain" -TimeoutSec 5 | Out-Null
    } catch {
        Write-Host "Could not send report: $($_.Exception.Message)"
    }
}

function Assert-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        $tempScript = Join-Path $env:TEMP "windows-deploy-fleet-tracker.ps1"
        Write-Host "Opening an Administrator PowerShell window..."
        Invoke-WebRequest -Uri "$BaseUrl/tools/windows-deploy-fleet-tracker.ps1" -OutFile $tempScript
        Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$tempScript`""
        Write-Host "Accept the Windows UAC prompt. The elevated window will continue the deploy."
        exit 0
    }
}

function Get-TailscaleExe {
    $cmd = Get-Command tailscale.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        "$env:ProgramFiles\Tailscale\tailscale.exe",
        "${env:ProgramFiles(x86)}\Tailscale\tailscale.exe"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return $candidate }
    }

    throw "Could not find tailscale.exe. Install Tailscale first, then rerun this script."
}

function Get-PythonExe {
    $py = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($py) {
        & $py.Source -3 --version *> $null
        if ($LASTEXITCODE -eq 0) { return "$($py.Source)|-3" }
    }

    $python = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($python -and ($python.Source -notlike "*\Microsoft\WindowsApps\python.exe")) {
        & $python.Source --version *> $null
        if ($LASTEXITCODE -eq 0) { return "$($python.Source)|" }
    }

    $knownPaths = @(
        "$env:LocalAppData\Programs\Python\Python313\python.exe",
        "$env:LocalAppData\Programs\Python\Python312\python.exe",
        "$env:LocalAppData\Microsoft\WindowsApps\python3.exe",
        "$env:ProgramFiles\Python313\python.exe",
        "$env:ProgramFiles\Python312\python.exe",
        "$env:ProgramFiles\WindowsApps\PythonSoftwareFoundation.Python.3.13_*\python.exe",
        "$env:ProgramFiles\WindowsApps\PythonSoftwareFoundation.Python.3.12_*\python.exe"
    )

    foreach ($candidate in $knownPaths) {
        $matches = Get-Item $candidate -ErrorAction SilentlyContinue
        foreach ($match in $matches) {
            if (Test-Path $match.FullName) { return "$($match.FullName)|" }
        }
    }

    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "Python is not installed and winget is unavailable. Install Python 3, then rerun this script."
    }

    Log "Installing Python 3 with winget..."
    & $winget.Source install -e --id Python.Python.3.13 --accept-package-agreements --accept-source-agreements --silent

    foreach ($candidate in $knownPaths) {
        $matches = Get-Item $candidate -ErrorAction SilentlyContinue
        foreach ($match in $matches) {
            if (Test-Path $match.FullName) { return "$($match.FullName)|" }
        }
    }

    $py = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($py) {
        & $py.Source -3 --version *> $null
        if ($LASTEXITCODE -eq 0) { return "$($py.Source)|-3" }
    }

    throw "Python install completed, but python.exe/py.exe was not found in PATH. Open a new PowerShell and rerun this script."
}

function Install-Files {
    Log "Installing Fleet Tracker files to $AppDir..."
    New-Item -ItemType Directory -Path $ServerDir -Force | Out-Null
    New-Item -ItemType Directory -Path $PublicDir -Force | Out-Null
    New-Item -ItemType Directory -Path $DataDir -Force | Out-Null

    Invoke-WebRequest -Uri "$BaseUrl/server/fleet_server.py" -OutFile (Join-Path $ServerDir "fleet_server.py")
    Invoke-WebRequest -Uri "$BaseUrl/server/public/index.html" -OutFile (Join-Path $PublicDir "index.html")
}

function Install-Launcher {
    $pythonSpec = Get-PythonExe
    $parts = $pythonSpec -split "\|", 2
    $pythonExe = $parts[0]
    $pythonArgs = ""
    if ($parts.Count -gt 1) {
        $pythonArgs = $parts[1]
    }

    if ($pythonArgs) {
        $pythonCommand = "`"$pythonExe`" $pythonArgs"
    } else {
        $pythonCommand = "`"$pythonExe`""
    }
    if ([string]::IsNullOrWhiteSpace($pythonExe)) {
        throw "Could not determine a usable Python executable."
    }
    Log "Using Python command: $pythonCommand"

    $launcherBody = @"
`$ErrorActionPreference = "Stop"
Set-Location "$AppDir"
& $pythonCommand "$ServerDir\fleet_server.py" --host 127.0.0.1 --port 8081
"@

    Set-Content -Path $Launcher -Value $launcherBody -Encoding UTF8
}

function Restart-Tracker {
    Log "Starting Fleet Tracker receiver on 127.0.0.1:8081..."
    Get-CimInstance Win32_Process |
        Where-Object { $_.CommandLine -like "*fleet_server.py*" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }

    $startupDir = [Environment]::GetFolderPath("Startup")
    $startupCommand = Join-Path $startupDir "FleetTrackerServer.cmd"
    $cmdBody = "@echo off`r`npowershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$Launcher`"`r`n"
    Set-Content -Path $startupCommand -Value $cmdBody -Encoding ASCII

    Start-Process -FilePath "powershell.exe" -WindowStyle Hidden -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$Launcher`""
    Start-Sleep -Seconds 3
}

function Configure-Funnel {
    $tailscale = Get-TailscaleExe
    Log "Setting Tailscale hostname and Funnel..."
    & $tailscale set --hostname=ops-main-01
    & $tailscale funnel --bg --yes http://127.0.0.1:8081
}

function Test-LocalEndpoint {
    Log "Testing local health endpoint..."
    $health = Invoke-RestMethod -Uri "http://127.0.0.1:8081/health"
    Log "Local receiver OK. Devices: $($health.devices)"
}

Assert-Admin
try {
    Log "=== Fleet Tracker Deploy ==="
    Log "Time: $(Get-Date -Format o)"
    Log "Computer: $env:COMPUTERNAME"
    Log "User: $env:USERNAME"
    Install-Files
    Install-Launcher
    Restart-Tracker
    Configure-Funnel
    Test-LocalEndpoint

    Log ""
    Log "Fleet Tracker server is installed."
    Log "Dashboard: https://ops-main-01.tail58e171.ts.net/"
    Log "Telemetry POST: https://ops-main-01.tail58e171.ts.net/api/fleet/telemetry"
} catch {
    Log "DEPLOY FAILED: $($_.Exception.Message)"
    throw
} finally {
    Send-Report
}
