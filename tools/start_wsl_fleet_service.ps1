$ErrorActionPreference = "Stop"

wsl -d Ubuntu -u root -- bash /mnt/c/ops/fleet-tracker/deploy_wsl_fleet_service.sh
wsl -d Ubuntu -u root -- bash -lc "systemctl start fleet-tracker.service && systemctl is-active fleet-tracker.service && for i in 1 2 3 4 5; do curl -fsS http://127.0.0.1:8081/api/fleet/health && exit 0; sleep 1; done; exit 1"
