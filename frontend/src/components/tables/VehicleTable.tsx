"use client";

import { MoreHorizontal, Search } from "lucide-react";
import { useMemo, useState } from "react";

import { vehicles } from "@/data/mockData";
import { statusTone } from "@/lib/utils";

import { Button } from "../ui/Button";
import { FeatureBadgeList } from "../ui/FeatureBadgeList";
import { SectionCard } from "../ui/SectionCard";

export function VehicleTable({
  onSelectVehicle,
  onAddVehicle,
}: {
  onSelectVehicle?: (vehicleId: string) => void;
  onAddVehicle?: () => void;
}) {
  const [query, setQuery] = useState("");

  const filtered = useMemo(() => {
    const needle = query.toLowerCase();
    return vehicles.filter((vehicle) =>
      [
        vehicle.name,
        vehicle.id,
        vehicle.plate,
        vehicle.type,
        vehicle.assignedDriver,
        vehicle.region,
      ]
        .join(" ")
        .toLowerCase()
        .includes(needle),
    );
  }, [query]);

  return (
    <SectionCard className="overflow-hidden">
      <div className="flex flex-wrap items-center justify-between gap-3 border-b border-brand-line px-5 py-4">
        <div>
          <h3 className="text-base font-semibold text-brand-ink">Vehicles</h3>
          <p className="text-sm text-slate-500">Searchable fleet registry with device assignment context</p>
        </div>
        <div className="flex flex-wrap items-center gap-2">
          <div className="relative">
            <Search size={16} className="absolute left-3 top-1/2 -translate-y-1/2 text-slate-400" />
            <input
              value={query}
              onChange={(event) => setQuery(event.target.value)}
              placeholder="Search vehicles"
              className="h-10 rounded-md border border-brand-line bg-brand-cloud pl-9 pr-3 text-sm"
            />
          </div>
          <Button onClick={onAddVehicle}>Add Vehicle</Button>
        </div>
      </div>

      <div className="overflow-x-auto">
        <table className="min-w-full text-left text-sm">
          <thead className="bg-brand-cloud text-slate-500">
            <tr>
              {[
                "Vehicle",
                "Vehicle ID",
                "Plate",
                "Type",
                "Assigned Driver",
                "Service Region",
                "Status",
                "GPS",
                "Device",
                "Enabled Features",
                "Last Seen",
                "Actions",
              ].map((header) => (
                <th key={header} className="px-5 py-3 font-semibold">
                  {header}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {filtered.map((vehicle) => (
              <tr
                key={vehicle.id}
                className="border-t border-brand-line hover:bg-brand-cloud/70"
              >
                <td className="px-5 py-4">
                  <button
                    className="text-left"
                    onClick={() => onSelectVehicle?.(vehicle.id)}
                  >
                    <div className="font-semibold text-brand-ink">{vehicle.name}</div>
                    <div className="text-xs text-slate-500">
                      {vehicle.make} {vehicle.model} {vehicle.year}
                    </div>
                  </button>
                </td>
                <td className="px-5 py-4 text-brand-text">{vehicle.id}</td>
                <td className="px-5 py-4 text-brand-text">{vehicle.plate}</td>
                <td className="px-5 py-4 text-brand-text">{vehicle.type}</td>
                <td className="px-5 py-4 text-brand-text">{vehicle.assignedDriver}</td>
                <td className="px-5 py-4 text-brand-text">{vehicle.region}</td>
                <td className="px-5 py-4">
                  <span className={`rounded-full border px-2 py-1 text-xs font-semibold ${statusTone(vehicle.status)}`}>
                    {vehicle.status}
                  </span>
                </td>
                <td className="px-5 py-4">
                  <span className={`rounded-full border px-2 py-1 text-xs font-semibold ${statusTone(vehicle.gpsOnline ? "online" : "offline")}`}>
                    {vehicle.gpsOnline ? "Online" : "Offline"}
                  </span>
                </td>
                <td className="px-5 py-4">
                  <span className={`rounded-full border px-2 py-1 text-xs font-semibold ${statusTone(vehicle.deviceStatus)}`}>
                    {vehicle.deviceStatus}
                  </span>
                </td>
                <td className="px-5 py-4">
                  <FeatureBadgeList items={vehicle.enabledFeatures} />
                </td>
                <td className="px-5 py-4 text-brand-text">{vehicle.lastSeen}</td>
                <td className="px-5 py-4">
                  <button className="rounded-md border border-brand-line p-2">
                    <MoreHorizontal size={16} />
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </SectionCard>
  );
}
