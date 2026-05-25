$ErrorActionPreference = "Stop"

Write-Output "=== Identity ==="
whoami

Write-Output "=== Admin Check ==="
$currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
Write-Output ("IsInAdministratorRole={0}" -f $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))

Write-Output "=== Groups ==="
whoami /groups
