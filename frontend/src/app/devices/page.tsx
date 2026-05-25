"use client";

import { Cpu, RadioTower, SlidersHorizontal, Zap } from "lucide-react";
import { useEffect, useState } from "react";

import { Button } from "@/components/ui/Button";
import { SectionCard } from "@/components/ui/SectionCard";
import { TrackingProfileCard } from "@/components/ui/TrackingProfileCard";
import { buildPrototypeDevice, useLiveTracker } from "@/lib/liveTracker";
import { useWorkspace } from "@/lib/workspace";

const configTiles = [
  "Moving update interval",
  "Stopped update interval",
  "Idle timeout",
  "Deep sleep timeout",
  "Wake on motion",
  "Wake on ignition",
  "Overnight parked mode",
  "Heartbeat interval",
  "Battery saver mode",
  "Data saver mode",
  "Firmware updates",
  "Diagnostics",
];

export default function DevicesPage() {
  const { liveTracker, liveError } = useLiveTracker();
  const liveDevice = buildPrototypeDevice(liveTracker);
  const { state, assignTrackerToVehicle } = useWorkspace();
  const [pendingVehicleId, setPendingVehicleId] = useState(state.trackerAssignmentVehicleId ?? "");
  const [assignmentSaved, setAssignmentSaved] = useState(false);
  const assignedVehicle =
    state.vehicles.find((vehicle) => vehicle.id === state.trackerAssignmentVehicleId)?.name ?? "Not assigned";

  useEffect(() => {
    setPendingVehicleId(state.trackerAssignmentVehicleId ?? "");
  }, [state.trackerAssignmentVehicleId]);

  const deviceKpis = [
    { label: "Online devices", value: liveError ? "0" : "1", icon: RadioTower },
    { label: "Pending firmware updates", value: "0", icon: Cpu },
    { label: "Profiles using smart sleep", value: "1", icon: Zap },
  ] as const;

  return (
    <div className="space-y-5">
      <div>
        <p className="text-sm font-semibold uppercase text-brand-forest">Devices</p>
        <h1 className="mt-1 text-3xl font-bold text-brand-ink">Devices</h1>
        <p className="mt-2 text-sm text-slate-500">
          Review tracker health, assignment, and reporting behavior from one place.
        </p>
      </div>

      <div className="grid gap-4 md:grid-cols-3">
        {deviceKpis.map(({ label, value, icon: Icon }) => {
          return (
            <SectionCard key={label} className="p-5">
              <div className="flex items-center gap-3">
                <div className="rounded-md bg-brand-mint p-3 text-brand-forest">
                  <Icon size={18} />
                </div>
                <div>
                  <p className="text-xs font-semibold uppercase text-slate-500">{label}</p>
                  <p className="mt-1 text-2xl font-bold text-brand-ink">{value}</p>
                </div>
              </div>
            </SectionCard>
          );
        })}
      </div>

      <div className="grid gap-5 2xl:grid-cols-[minmax(0,1fr)_420px]">
        <div className="space-y-5">
          <SectionCard className="overflow-hidden">
            <div className="border-b border-brand-line px-5 py-4">
              <h3 className="text-base font-semibold text-brand-ink">Tracker details</h3>
              <p className="text-sm text-slate-500">Tracker currently connected to this account.</p>
            </div>
            <div className="grid gap-3 p-5 md:grid-cols-2 xl:grid-cols-3">
              {[
                ["Device ID", liveDevice.id],
                [
                  "Assignment",
                  assignedVehicle,
                ],
                ["SIM", liveDevice.simNumber],
                ["Firmware", liveDevice.firmwareVersion],
                ["Battery", `${liveDevice.batteryLevel}%`],
                ["Signal", `${liveDevice.signalStrength}%`],
                ["Location", liveDevice.gpsLock ? "Available" : "Waiting for GPS"],
                ["Motion", liveDevice.motionState],
                ["Last update", liveDevice.lastPing],
              ].map(([label, value]) => (
                <div key={label} className="rounded-md border border-brand-line bg-brand-cloud px-4 py-4">
                  <p className="text-xs font-semibold uppercase text-slate-500">{label}</p>
                  <p className="mt-1 text-sm font-semibold text-brand-ink">{value}</p>
                </div>
              ))}
            </div>
            <div className="border-t border-brand-line px-5 py-4">
              <div className="flex flex-wrap items-center gap-3">
                <select
                  className="h-10 min-w-[220px] rounded-md border border-brand-line bg-white px-3 text-sm"
                  value={pendingVehicleId}
                  onChange={(event) => {
                    setPendingVehicleId(event.target.value);
                    setAssignmentSaved(false);
                  }}
                  disabled={state.vehicles.length === 0}
                >
                  <option value="">Assign tracker to vehicle</option>
                  {state.vehicles.map((vehicle) => (
                    <option key={vehicle.id} value={vehicle.id}>
                      {vehicle.name}
                    </option>
                  ))}
                </select>
                <Button
                  variant="secondary"
                  disabled={state.vehicles.length === 0 || !pendingVehicleId}
                  onClick={() => {
                    assignTrackerToVehicle(pendingVehicleId);
                    setAssignmentSaved(true);
                  }}
                >
                  Save Assignment
                </Button>
              </div>
              {state.vehicles.length === 0 && (
                <p className="mt-2 text-sm text-slate-500">Add a vehicle first, then link tracker-001 here.</p>
              )}
              {state.vehicles.length > 0 && !pendingVehicleId && (
                <p className="mt-2 text-sm text-slate-500">Choose the vehicle you want tracker-001 assigned to.</p>
              )}
              {assignmentSaved && (
                <p className="mt-2 text-sm text-brand-forest">Tracker assignment saved.</p>
              )}
              {!assignmentSaved && state.trackerAssignmentVehicleId && state.vehicles.length > 0 && (
                <p className="mt-2 text-sm text-slate-500">Current assignment: {assignedVehicle}.</p>
              )}
            </div>
          </SectionCard>
          <SectionCard className="p-5">
            <div className="flex items-center gap-3">
              <div className="rounded-md bg-brand-cloud p-3 text-brand-navy">
                <SlidersHorizontal size={18} />
              </div>
              <div>
                <h2 className="text-base font-semibold text-brand-ink">Tracker Settings</h2>
                <p className="text-sm text-slate-500">
                  Reporting and power settings for this tracker.
                </p>
              </div>
            </div>
            <div className="mt-5 grid gap-3 sm:grid-cols-2 xl:grid-cols-3">
              {configTiles.map((tile) => (
                <div key={tile} className="rounded-md border border-brand-line px-4 py-4">
                  <p className="text-sm font-semibold text-brand-ink">{tile}</p>
                  <p className="mt-1 text-xs text-slate-500">Keep installed trackers consistent across the fleet.</p>
                </div>
              ))}
            </div>
          </SectionCard>
        </div>
        <TrackingProfileCard device={liveDevice} />
      </div>
    </div>
  );
}
