"use client";

import { useMemo, useState } from "react";
import { X } from "lucide-react";

import { Button } from "@/components/ui/Button";
import { SectionCard } from "@/components/ui/SectionCard";
import { useWorkspace } from "@/lib/workspace";

function FilterContent() {
  const { state, serviceArea, hasServiceArea } = useWorkspace();
  const [query, setQuery] = useState("");
  const [dateRange, setDateRange] = useState("Last 24 hours");
  const [selected, setSelected] = useState<Record<string, string[]>>({});

  const filterGroups = useMemo(
    () => [
      {
        title: "Vehicle status",
        options: ["Moving", "Idle", "Parked", "Offline"],
      },
      {
        title: "Vehicle type",
        options: ["Van", "Service Truck", "Car", "SUV", "Tow Truck", "Motorcycle", "Trailer"],
      },
      {
        title: "Assigned driver",
        options: state.drivers.length > 0 ? state.drivers.map((driver) => driver.name) : ["No drivers on file"],
      },
      {
        title: "Region",
        options: hasServiceArea
          ? [serviceArea.label, "Yard", "Field installs", "Customer locations"]
          : ["Yard", "Field installs", "Customer locations"],
      },
      {
        title: "Enabled features",
        options: ["GPS Tracking", "Live Location", "Geofencing", "Battery Monitoring"],
      },
      {
        title: "Device status",
        options: ["Online", "Needs Attention", "Offline"],
      },
      {
        title: "Sleep state",
        options: ["Awake", "Idle Watch", "Deep Sleep"],
      },
      {
        title: "Alert type",
        options: ["Speeding", "Idle", "Offline", "Geofence Exit"],
      },
    ],
    [hasServiceArea, serviceArea.label, state.drivers],
  );

  function toggleOption(group: string, option: string) {
    setSelected((current) => {
      const existing = current[group] ?? [];
      const next = existing.includes(option)
        ? existing.filter((item) => item !== option)
        : [...existing, option];
      return { ...current, [group]: next };
    });
  }

  return (
    <>
      <div className="rounded-md border border-brand-line bg-brand-cloud p-3">
        <label className="text-xs font-semibold uppercase text-slate-500">Search</label>
        <input
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          className="mt-2 h-10 w-full rounded-md border border-brand-line bg-white px-3 text-sm outline-none"
          placeholder="Search vehicles or devices"
        />
      </div>
      <div className="rounded-md border border-brand-line bg-brand-cloud p-3">
        <label className="text-xs font-semibold uppercase text-slate-500">Date range</label>
        <div className="mt-2 grid gap-2">
          {["Last 24 hours", "Last 7 days", "Last 30 days"].map((option) => (
            <button
              key={option}
              onClick={() => setDateRange(option)}
              className={`rounded-md border px-3 py-2 text-left text-sm font-medium ${
                dateRange === option ? "border-brand-forest bg-white text-brand-ink" : "border-brand-line bg-white text-brand-text"
              }`}
            >
              {option}
            </button>
          ))}
        </div>
      </div>
      {filterGroups.map((group) => (
        <div key={group.title} className="rounded-md border border-brand-line bg-white p-3">
          <p className="text-xs font-semibold uppercase text-slate-500">{group.title}</p>
          <div className="mt-3 space-y-2">
            {group.options.map((option) => (
              <label key={option} className="flex items-center gap-2 text-sm text-brand-text">
                <input
                  type="checkbox"
                  checked={(selected[group.title] ?? []).includes(option)}
                  onChange={() => toggleOption(group.title, option)}
                  className="h-4 w-4 rounded border-brand-line text-brand-forest"
                />
                <span>{option}</span>
              </label>
            ))}
          </div>
        </div>
      ))}
      <Button
        variant="ghost"
        className="w-full"
        onClick={() => {
          setQuery("");
          setDateRange("Last 24 hours");
          setSelected({});
        }}
      >
        Reset Filters
      </Button>
    </>
  );
}

export function StickyFilterPanel() {
  return (
    <div className="sticky top-[96px] hidden xl:block">
      <SectionCard className="max-h-[calc(100vh-120px)] overflow-y-auto p-4 scrollbar-thin">
        <div className="mb-4 flex items-center justify-between">
          <div>
            <h3 className="text-sm font-semibold text-brand-ink">Fleet Filters</h3>
            <p className="text-xs text-slate-500">Filter vehicles, devices, and alerts</p>
          </div>
        </div>
        <div className="space-y-3">
          <FilterContent />
        </div>
      </SectionCard>
    </div>
  );
}

export function MobileFilterDrawer({
  open,
  onClose,
}: {
  open: boolean;
  onClose: () => void;
}) {
  if (!open) return null;

  return (
    <div className="fixed inset-0 z-40 bg-brand-ink/40 xl:hidden">
      <div className="absolute right-0 top-0 h-full w-full max-w-sm overflow-y-auto bg-white p-4 shadow-panel">
        <div className="mb-4 flex items-center justify-between">
          <div>
            <h3 className="text-base font-semibold text-brand-ink">Filters</h3>
            <p className="text-sm text-slate-500">Refine vehicles, devices, and alerts</p>
          </div>
          <button onClick={onClose} className="rounded-md border border-brand-line p-2">
            <X size={18} />
          </button>
        </div>
        <div className="space-y-3">
          <FilterContent />
        </div>
      </div>
    </div>
  );
}
