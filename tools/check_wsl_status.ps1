$ErrorActionPreference = "Stop"

Write-Output "=== OS ==="
Get-ComputerInfo | Select-Object WindowsProductName, WindowsVersion, OsBuildNumber | Format-List

Write-Output "=== WSL Status ==="
try {
    wsl --status
} catch {
    Write-Output "wsl --status failed: $($_.Exception.Message)"
}

Write-Output "=== Installed Distros ==="
try {
    wsl -l -v
} catch {
    Write-Output "wsl -l -v failed: $($_.Exception.Message)"
}

Write-Output "=== Optional Features ==="
Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux, VirtualMachinePlatform |
    Select-Object FeatureName, State |
    Format-Table -AutoSize
