"use client";

import { useMemo, useState } from "react";

import { drivers } from "@/data/mockData";
import { statusTone } from "@/lib/utils";

import { SectionCard } from "../ui/SectionCard";

export function DriversTable() {
  const [query, setQuery] = useState("");

  const rows = useMemo(() => {
    const needle = query.toLowerCase();
    return drivers.filter((driver) =>
      [driver.name, driver.phone, driver.assignedVehicle, driver.region]
        .join(" ")
        .toLowerCase()
        .includes(needle),
    );
  }, [query]);

  return (
    <SectionCard className="overflow-hidden">
      <div className="flex items-center justify-between gap-3 border-b border-brand-line px-5 py-4">
        <div>
          <h3 className="text-base font-semibold text-brand-ink">Drivers</h3>
          <p className="text-sm text-slate-500">Operator directory with assignment and region coverage</p>
        </div>
        <input
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          placeholder="Search drivers"
          className="h-10 rounded-md border border-brand-line bg-brand-cloud px-3 text-sm"
        />
      </div>
      <div className="overflow-x-auto">
        <table className="min-w-full text-left text-sm">
          <thead className="bg-brand-cloud text-slate-500">
            <tr>
              {["Driver", "Phone", "Assigned Vehicle", "Status", "Region", "License", "Notes"].map((header) => (
                <th key={header} className="px-5 py-3 font-semibold">
                  {header}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((driver) => (
              <tr key={driver.id} className="border-t border-brand-line">
                <td className="px-5 py-4 font-semibold text-brand-ink">{driver.name}</td>
                <td className="px-5 py-4 text-brand-text">{driver.phone}</td>
                <td className="px-5 py-4 text-brand-text">{driver.assignedVehicle}</td>
                <td className="px-5 py-4">
                  <span className={`rounded-full border px-2 py-1 text-xs font-semibold ${statusTone(driver.status)}`}>
                    {driver.status}
                  </span>
                </td>
                <td className="px-5 py-4 text-brand-text">{driver.region}</td>
                <td className="px-5 py-4 text-brand-text">{driver.license}</td>
                <td className="px-5 py-4 text-brand-text">{driver.notes}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </SectionCard>
  );
}
