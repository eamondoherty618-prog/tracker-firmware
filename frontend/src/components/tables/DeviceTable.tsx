"use client";

import { useMemo, useState } from "react";

import { devices, vehicles } from "@/data/mockData";
import { statusTone } from "@/lib/utils";

import { FeatureBadgeList } from "../ui/FeatureBadgeList";
import { SectionCard } from "../ui/SectionCard";

export function DeviceTable({ onSelectDevice }: { onSelectDevice?: (deviceId: string) => void }) {
  const [query, setQuery] = useState("");

  const rows = useMemo(() => {
    const needle = query.toLowerCase();
    return devices
      .map((device) => ({
        ...device,
        vehicleName: vehicles.find((vehicle) => vehicle.id === device.assignedVehicleId)?.name ?? "Unassigned",
      }))
      .filter((device) =>
        [device.id, device.vehicleName, device.simNumber, device.reportingProfile]
          .join(" ")
          .toLowerCase()
          .includes(needle),
      );
  }, [query]);

  return (
    <SectionCard className="overflow-hidden">
      <div className="flex items-center justify-between gap-3 border-b border-brand-line px-5 py-4">
        <div>
          <h3 className="text-base font-semibold text-brand-ink">Devices</h3>
          <p className="text-sm text-slate-500">Hardware health, firmware posture, and reporting behavior</p>
        </div>
        <input
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          placeholder="Search devices"
          className="h-10 rounded-md border border-brand-line bg-brand-cloud px-3 text-sm"
        />
      </div>
      <div className="overflow-x-auto">
        <table className="min-w-full text-left text-sm">
          <thead className="bg-brand-cloud text-slate-500">
            <tr>
              {[
                "Device ID",
                "Assigned Vehicle",
                "SIM Number",
                "Firmware",
                "Battery",
                "Signal",
                "GPS Lock",
                "Sleep State",
                "Motion",
                "Last Ping",
                "Status",
                "Reporting Profile",
                "Enabled Features",
              ].map((header) => (
                <th key={header} className="px-5 py-3 font-semibold">
                  {header}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((device) => (
              <tr
                key={device.id}
                className="border-t border-brand-line hover:bg-brand-cloud/70"
                onClick={() => onSelectDevice?.(device.id)}
              >
                <td className="px-5 py-4 font-semibold text-brand-ink">{device.id}</td>
                <td className="px-5 py-4 text-brand-text">{device.vehicleName}</td>
                <td className="px-5 py-4 text-brand-text">{device.simNumber}</td>
                <td className="px-5 py-4 text-brand-text">{device.firmwareVersion}</td>
                <td className="px-5 py-4 text-brand-text">{device.batteryLevel}%</td>
                <td className="px-5 py-4 text-brand-text">{device.signalStrength}%</td>
                <td className="px-5 py-4 text-brand-text">{device.gpsLock ? "Locked" : "Searching"}</td>
                <td className="px-5 py-4 text-brand-text">{device.sleepState}</td>
                <td className="px-5 py-4 text-brand-text">{device.motionState}</td>
                <td className="px-5 py-4 text-brand-text">{device.lastPing}</td>
                <td className="px-5 py-4">
                  <span className={`rounded-full border px-2 py-1 text-xs font-semibold ${statusTone(device.online ? "online" : "offline")}`}>
                    {device.online ? "Online" : "Offline"}
                  </span>
                </td>
                <td className="px-5 py-4 text-brand-text">{device.reportingProfile}</td>
                <td className="px-5 py-4">
                  <FeatureBadgeList items={device.enabledFeatures} />
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </SectionCard>
  );
}
