$ErrorActionPreference = "Stop"

Write-Output "=== Enable Microsoft-Windows-Subsystem-Linux ==="
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart

Write-Output "=== Enable VirtualMachinePlatform ==="
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart

Write-Output "=== Feature States After Attempt ==="
Get-WindowsOptionalFeature -Online -FeatureName "Microsoft-Windows-Subsystem-Linux" |
    Select-Object FeatureName, State |
    Format-Table -AutoSize

Get-WindowsOptionalFeature -Online -FeatureName "VirtualMachinePlatform" |
    Select-Object FeatureName, State |
    Format-Table -AutoSize
