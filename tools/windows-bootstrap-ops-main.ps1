$ErrorActionPreference = "Stop"

$BootstrapUrl = "http://100.105.91.8:8765/tools/windows-bootstrap-ops-main.ps1"
$ServerName = "ops-main-01"
$MacPublicKey = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIE6j63sZCGr0pM0oinonZ/cf1xYaThwBdJxqLP915WyW eamon@mac-to-garageaudio"

function Assert-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        $tempScript = Join-Path $env:TEMP "windows-bootstrap-ops-main.ps1"
        Write-Host "This PowerShell session is not elevated. Opening an Administrator PowerShell window..."
        Invoke-WebRequest -Uri $BootstrapUrl -OutFile $tempScript
        Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$tempScript`""
        Write-Host "Accept the Windows UAC prompt. The elevated window will continue the bootstrap."
        exit 0
    }
}

function Get-TailscaleExe {
    $cmd = Get-Command tailscale.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = @(
        "$env:ProgramFiles\Tailscale\tailscale.exe",
        "${env:ProgramFiles(x86)}\Tailscale\tailscale.exe"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    throw "Could not find tailscale.exe. Install Tailscale first, then rerun this bootstrap."
}

function Set-TailscaleName {
    $tailscale = Get-TailscaleExe
    Write-Host "Setting Tailscale hostname to $ServerName..."
    & $tailscale set "--hostname=$ServerName"
}

function Set-WindowsName {
    $currentName = $env:COMPUTERNAME
    if ($currentName -ieq $ServerName) {
        Write-Host "Windows computer name is already $ServerName."
        return
    }

    Write-Host "Renaming Windows computer from $currentName to $ServerName..."
    Rename-Computer -NewName $ServerName -Force
    Write-Host "A reboot is needed for the Windows computer name to fully apply."
}

function Enable-OpenSsh {
    Write-Host "Enabling OpenSSH Server..."
    $capability = Get-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
    if ($capability.State -ne "Installed") {
        Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
    }

    Start-Service sshd
    Set-Service -Name sshd -StartupType Automatic

    $ruleName = "OpenSSH-Server-In-TCP"
    $rule = Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
    if (-not $rule) {
        New-NetFirewallRule -Name $ruleName -DisplayName "OpenSSH Server (sshd)" -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 | Out-Null
    } else {
        Enable-NetFirewallRule -Name $ruleName | Out-Null
    }
}

function Add-MacSshKey {
    Write-Host "Adding Mac SSH key for administrator access..."
    $sshDir = "C:\ProgramData\ssh"
    $authorizedKeys = Join-Path $sshDir "administrators_authorized_keys"

    New-Item -ItemType Directory -Path $sshDir -Force | Out-Null
    if (-not (Test-Path $authorizedKeys)) {
        New-Item -ItemType File -Path $authorizedKeys -Force | Out-Null
    }

    $existing = Get-Content $authorizedKeys -ErrorAction SilentlyContinue
    if ($existing -notcontains $MacPublicKey) {
        Add-Content -Path $authorizedKeys -Value $MacPublicKey
    }

    icacls.exe $authorizedKeys /inheritance:r /grant "Administrators:F" /grant "SYSTEM:F" | Out-Null
}

function Ensure-Python {
    $python = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($python) {
        Write-Host "Python is already available at $($python.Source)."
        return
    }

    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) {
        Write-Warning "Python is not installed and winget is unavailable. Install Python 3 manually before running the tracker server."
        return
    }

    Write-Host "Installing Python 3 with winget..."
    & $winget.Source install -e --id Python.Python.3.13 --accept-package-agreements --accept-source-agreements
}

function Prepare-OpsFolder {
    New-Item -ItemType Directory -Path "C:\ops" -Force | Out-Null
}

Assert-Admin
Set-TailscaleName
Set-WindowsName
Enable-OpenSsh
Add-MacSshKey
Ensure-Python
Prepare-OpsFolder

Write-Host ""
Write-Host "Bootstrap complete."
Write-Host "Tailscale name target: $ServerName"
Write-Host "SSH should be reachable over Tailscale on port 22."
Write-Host "Restart Windows when convenient so the computer name fully changes."
