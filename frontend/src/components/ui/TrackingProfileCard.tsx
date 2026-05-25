import { Device } from "@/types";

import { SectionCard } from "./SectionCard";

export function TrackingProfileCard({ device }: { device: Device }) {
  const profileLabel = device.reportingProfile
    .split("-")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");

  const rows = [
    ["Moving update interval", device.movingUpdateInterval],
    ["Stopped update interval", device.stoppedUpdateInterval],
    ["Mark idle after", device.idleTimeout],
    ["Deep sleep after", device.deepSleepTimeout],
    ["Heartbeat while asleep", device.heartbeatInterval],
    ["Wake on motion", "Enabled"],
    ["Wake on ignition", "Available when wired"],
    ["Overnight parked mode", "Enabled"],
    ["Battery saver mode", "Enabled"],
    ["Data saver mode", "Balanced"],
  ];

  return (
    <SectionCard className="p-5">
      <div className="flex items-start justify-between gap-4">
        <div>
          <h3 className="text-base font-semibold text-brand-ink">Tracking Profile</h3>
          <p className="mt-1 text-sm text-slate-500">
            Smart power behavior for ESP32 + SIM tracker installs.
          </p>
        </div>
        <div className="rounded-md bg-brand-mint px-3 py-1 text-xs font-semibold text-brand-forest">
          {profileLabel}
        </div>
      </div>
      <div className="mt-5 grid gap-3 sm:grid-cols-2">
        {rows.map(([label, value]) => (
          <div key={label} className="rounded-md border border-brand-line bg-brand-cloud px-4 py-3">
            <p className="text-xs font-semibold uppercase text-slate-500">{label}</p>
            <p className="mt-1 text-sm font-semibold text-brand-ink">{value}</p>
          </div>
        ))}
      </div>
      <p className="mt-4 text-xs text-slate-500">
        These default intervals balance quick updates while driving with lower power use when parked.
      </p>
    </SectionCard>
  );
}
