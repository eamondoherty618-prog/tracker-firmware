$existing = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*fleet_server.py*" }
if ($existing) {
    $existing | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
}

Start-Process -FilePath py -ArgumentList "-3", "C:\ops\fleet-tracker\server\fleet_server.py", "--host", "127.0.0.1", "--port", "8081" -WindowStyle Hidden
Start-Sleep -Seconds 2

Write-Output "RESTARTED"

try {
    (Invoke-WebRequest -UseBasicParsing -Method Head -Uri "http://127.0.0.1:8081/").StatusCode
} catch {
    $_.Exception.Response.StatusCode.value__
}

try {
    (Invoke-WebRequest -UseBasicParsing -Method Head -Uri "http://127.0.0.1:8081/api/fleet/health").StatusCode
} catch {
    $_.Exception.Response.StatusCode.value__
}
