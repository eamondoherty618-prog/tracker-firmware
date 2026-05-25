"use client";

import { useMemo, useState } from "react";

import { trips } from "@/data/mockData";
import { statusTone } from "@/lib/utils";

import { SectionCard } from "../ui/SectionCard";

export function TripsTable() {
  const [query, setQuery] = useState("");

  const rows = useMemo(() => {
    const needle = query.toLowerCase();
    return trips.filter((trip) =>
      [trip.vehicle, trip.driver, trip.region, trip.status]
        .join(" ")
        .toLowerCase()
        .includes(needle),
    );
  }, [query]);

  return (
    <SectionCard className="overflow-hidden">
      <div className="flex items-center justify-between gap-3 border-b border-brand-line px-5 py-4">
        <div>
          <h3 className="text-base font-semibold text-brand-ink">Trips</h3>
          <p className="text-sm text-slate-500">Searchable trip ledger ready for compliance and route analytics</p>
        </div>
        <input
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          placeholder="Search trips"
          className="h-10 rounded-md border border-brand-line bg-brand-cloud px-3 text-sm"
        />
      </div>
      <div className="overflow-x-auto">
        <table className="min-w-full text-left text-sm">
          <thead className="bg-brand-cloud text-slate-500">
            <tr>
              {["Vehicle", "Driver", "Start Time", "End Time", "Distance", "Duration", "Region", "Trip Status"].map((header) => (
                <th key={header} className="px-5 py-3 font-semibold">
                  {header}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((trip) => (
              <tr key={trip.id} className="border-t border-brand-line">
                <td className="px-5 py-4 font-semibold text-brand-ink">{trip.vehicle}</td>
                <td className="px-5 py-4 text-brand-text">{trip.driver}</td>
                <td className="px-5 py-4 text-brand-text">{trip.startTime}</td>
                <td className="px-5 py-4 text-brand-text">{trip.endTime}</td>
                <td className="px-5 py-4 text-brand-text">{trip.distance}</td>
                <td className="px-5 py-4 text-brand-text">{trip.duration}</td>
                <td className="px-5 py-4 text-brand-text">{trip.region}</td>
                <td className="px-5 py-4">
                  <span className={`rounded-full border px-2 py-1 text-xs font-semibold ${statusTone(trip.status)}`}>
                    {trip.status}
                  </span>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </SectionCard>
  );
}
