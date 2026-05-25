$ErrorActionPreference = "Stop"

$linuxUser = "eamon"
$linuxPassword = "codex-temp-pass"

Write-Output "=== Update packages ==="
wsl -d Ubuntu -u root -- bash -lc "apt-get update"

Write-Output "=== Install base tools ==="
wsl -d Ubuntu -u root -- bash -lc "DEBIAN_FRONTEND=noninteractive apt-get install -y sudo curl wget git unzip zip ca-certificates gnupg lsb-release build-essential python3 python3-pip python3-venv"

Write-Output "=== Create Linux user if missing ==="
wsl -d Ubuntu -u root -- bash -lc "id -u $linuxUser >/dev/null 2>&1 || useradd -m -s /bin/bash -G sudo $linuxUser"

Write-Output "=== Set temporary password ==="
$credentialLine = "${linuxUser}:${linuxPassword}"
wsl -d Ubuntu -u root -- bash -lc "echo '$credentialLine' | chpasswd"

Write-Output "=== Passwordless sudo for bootstrap user ==="
wsl -d Ubuntu -u root -- bash -lc "printf '$linuxUser ALL=(ALL) NOPASSWD:ALL\n' >/etc/sudoers.d/99-$linuxUser && chmod 440 /etc/sudoers.d/99-$linuxUser"

Write-Output "=== Set default WSL user ==="
wsl --manage Ubuntu --set-default-user $linuxUser

Write-Output "=== Verify ==="
wsl -d Ubuntu -u root -- bash -lc "id $linuxUser"
wsl -d Ubuntu -- bash -lc "whoami && sudo -n true && python3 --version && git --version"

Write-Output "TEMP_PASSWORD=$linuxPassword"
