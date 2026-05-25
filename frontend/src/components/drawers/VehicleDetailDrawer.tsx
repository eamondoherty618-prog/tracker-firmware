"use client";

import { X } from "lucide-react";

import { devices, vehicles } from "@/data/mockData";

import { Badge } from "../ui/Badge";
import { EmptyState } from "../ui/EmptyState";
import { FeatureBadgeList } from "../ui/FeatureBadgeList";
import { SectionCard } from "../ui/SectionCard";

export function VehicleDetailDrawer({
  vehicleId,
  onClose,
}: {
  vehicleId?: string | null;
  onClose?: () => void;
}) {
  const vehicle = vehicles.find((item) => item.id === vehicleId);
  const device = devices.find((item) => item.assignedVehicleId === vehicleId);

  if (!vehicle) {
    return (
      <EmptyState
        title="Select a vehicle"
        description="Pick any vehicle from the map or registry to review assignment details, reporting behavior, and install notes."
      />
    );
  }

  return (
    <SectionCard className="p-5">
      <div className="flex items-start justify-between gap-3">
        <div>
          <p className="text-xs font-semibold uppercase text-brand-forest">Selected Vehicle</p>
          <h3 className="mt-1 text-lg font-semibold text-brand-ink">{vehicle.name}</h3>
          <p className="text-sm text-slate-500">
            {vehicle.make} {vehicle.model} · {vehicle.plate}
          </p>
        </div>
        <button onClick={onClose} className="rounded-md border border-brand-line p-2">
          <X size={16} />
        </button>
      </div>

      <div className="mt-5 grid gap-3 sm:grid-cols-2">
        {[
          ["Vehicle ID", vehicle.id],
          ["Assigned Driver", vehicle.assignedDriver],
          ["Service Region", vehicle.region],
          ["Hardware", vehicle.hardwareType],
          ["Device Assignment", vehicle.deviceAssignment],
          ["Last Seen", vehicle.lastSeen],
        ].map(([label, value]) => (
          <div key={label} className="rounded-md border border-brand-line bg-brand-cloud px-4 py-3">
            <p className="text-xs font-semibold uppercase text-slate-500">{label}</p>
            <p className="mt-1 text-sm font-semibold text-brand-ink">{value}</p>
          </div>
        ))}
      </div>

      <div className="mt-5 flex flex-wrap gap-2">
        <Badge tone={vehicle.status}>{vehicle.status}</Badge>
        <Badge tone={vehicle.gpsOnline ? "online" : "offline"}>
          GPS {vehicle.gpsOnline ? "Online" : "Offline"}
        </Badge>
        <Badge tone={vehicle.deviceStatus}>{vehicle.deviceStatus}</Badge>
      </div>

      <div className="mt-5">
        <p className="text-xs font-semibold uppercase text-slate-500">Enabled Features</p>
        <div className="mt-2">
          <FeatureBadgeList items={vehicle.enabledFeatures} />
        </div>
      </div>

      {device && (
        <div className="mt-5 rounded-md border border-brand-line px-4 py-4">
          <p className="text-xs font-semibold uppercase text-slate-500">Device Reporting</p>
          <div className="mt-3 grid gap-3 sm:grid-cols-2">
            <div>
              <p className="text-sm font-medium text-brand-text">Moving interval</p>
              <p className="text-sm font-semibold text-brand-ink">{device.movingUpdateInterval}</p>
            </div>
            <div>
              <p className="text-sm font-medium text-brand-text">Stopped interval</p>
              <p className="text-sm font-semibold text-brand-ink">{device.stoppedUpdateInterval}</p>
            </div>
            <div>
              <p className="text-sm font-medium text-brand-text">Deep sleep</p>
              <p className="text-sm font-semibold text-brand-ink">{device.deepSleepTimeout}</p>
            </div>
            <div>
              <p className="text-sm font-medium text-brand-text">Heartbeat</p>
              <p className="text-sm font-semibold text-brand-ink">{device.heartbeatInterval}</p>
            </div>
          </div>
        </div>
      )}

      <p className="mt-5 text-sm leading-6 text-slate-500">{vehicle.notes}</p>
    </SectionCard>
  );
}
