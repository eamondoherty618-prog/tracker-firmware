"use client";

import { X } from "lucide-react";

import { devices, vehicles } from "@/data/mockData";

import { EmptyState } from "../ui/EmptyState";
import { FeatureBadgeList } from "../ui/FeatureBadgeList";
import { SectionCard } from "../ui/SectionCard";
import { TrackingProfileCard } from "../ui/TrackingProfileCard";

export function DeviceDetailDrawer({
  deviceId,
  onClose,
}: {
  deviceId?: string | null;
  onClose?: () => void;
}) {
  const device = devices.find((item) => item.id === deviceId);
  const vehicle = vehicles.find((item) => item.id === device?.assignedVehicleId);

  if (!device) {
    return (
      <EmptyState
        title="Select a device"
        description="Choose a tracker to inspect firmware, SIM health, reporting intervals, sleep behavior, and future diagnostics."
      />
    );
  }

  return (
    <div className="space-y-5">
      <SectionCard className="p-5">
        <div className="flex items-start justify-between gap-3">
          <div>
            <p className="text-xs font-semibold uppercase text-brand-forest">Selected Device</p>
            <h3 className="mt-1 text-lg font-semibold text-brand-ink">{device.id}</h3>
            <p className="text-sm text-slate-500">{vehicle?.name ?? "Unassigned vehicle"}</p>
          </div>
          <button onClick={onClose} className="rounded-md border border-brand-line p-2">
            <X size={16} />
          </button>
        </div>
        <div className="mt-5 grid gap-3 sm:grid-cols-2">
          {[
            ["SIM Number", device.simNumber],
            ["Firmware", device.firmwareVersion],
            ["Battery", `${device.batteryLevel}%`],
            ["Signal", `${device.signalStrength}%`],
            ["GPS Lock", device.gpsLock ? "Locked" : "Searching"],
            ["Last Ping", device.lastPing],
            ["Sleep State", device.sleepState],
            ["Motion State", device.motionState],
          ].map(([label, value]) => (
            <div key={label} className="rounded-md border border-brand-line bg-brand-cloud px-4 py-3">
              <p className="text-xs font-semibold uppercase text-slate-500">{label}</p>
              <p className="mt-1 text-sm font-semibold text-brand-ink">{value}</p>
            </div>
          ))}
        </div>
        <div className="mt-5">
          <p className="text-xs font-semibold uppercase text-slate-500">Enabled Features</p>
          <div className="mt-2">
            <FeatureBadgeList items={device.enabledFeatures} />
          </div>
        </div>
      </SectionCard>
      <TrackingProfileCard device={device} />
    </div>
  );
}
