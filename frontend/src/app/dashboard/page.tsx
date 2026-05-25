"use client";

import {
  BatteryMedium,
  CheckCircle2,
  CircleDashed,
  LocateFixed,
  MapPinned,
  Plus,
  RadioTower,
  Route,
  ShieldCheck,
  SlidersHorizontal,
} from "lucide-react";
import { useMemo, useState } from "react";
import Link from "next/link";

import { AddVehicleModal } from "@/components/forms/AddVehicleModal";
import { SetupWorkspaceModal } from "@/components/forms/SetupWorkspaceModal";
import { MapView } from "@/components/map/MapView";
import { KPIStatCard } from "@/components/ui/KPIStatCard";
import { EmptyState } from "@/components/ui/EmptyState";
import { Button } from "@/components/ui/Button";
import { SectionCard } from "@/components/ui/SectionCard";
import { buildPrototypeDevice, useLiveTracker, useSetupChecklist } from "@/lib/liveTracker";
import { useWorkspace } from "@/lib/workspace";

function signalLabel(rssi?: number) {
  if (typeof rssi !== "number") return "--";
  if (rssi >= 24) return "Great";
  if (rssi >= 18) return "Strong";
  if (rssi >= 12) return "Medium";
  return "Weak";
}

function batteryPercent(mv?: number) {
  if (typeof mv !== "number") return "--";
  const percent = Math.round(((mv - 3300) / 900) * 100);
  return `${Math.max(0, Math.min(100, percent))}%`;
}

function formatProfileLabel(value: string) {
  return value
    .split("-")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

export default function DashboardPage() {
  const { state, serviceArea, hasServiceArea } = useWorkspace();
  const { liveTracker, liveError } = useLiveTracker();
  const liveDevice = buildPrototypeDevice(liveTracker);
  const checklist = useSetupChecklist(
    liveTracker,
    liveError,
    state.vehicles.length,
    Boolean(state.trackerAssignmentVehicleId),
  );
  const [companyOpen, setCompanyOpen] = useState(false);
  const [addVehicleOpen, setAddVehicleOpen] = useState(false);

  const kpis = useMemo(
    () => [
      { label: "Trackers online", value: liveError ? "0" : "1", delta: "Connected", tone: "green" as const },
      { label: "Vehicles", value: String(state.vehicles.length), delta: state.vehicles.length > 0 ? "In this account" : "Add your first vehicle", tone: "navy" as const },
      {
        label: "Location",
        value: liveTracker?.has_fix ? "Available" : "Waiting for GPS",
        delta: liveTracker?.has_fix ? "Coordinates coming in" : "Take the tracker outside to confirm",
        tone: "amber" as const,
      },
      { label: "Alerts", value: state.setupComplete ? "Available" : "Not configured", delta: "Set alert rules when ready", tone: "navy" as const },
      {
        label: "Service area",
        value: hasServiceArea ? serviceArea.label : "Not set",
        delta: hasServiceArea ? "Active" : "Choose your area",
        tone: "navy" as const,
      },
    ],
    [hasServiceArea, liveError, liveTracker?.has_fix, serviceArea.label, state.setupComplete, state.vehicles.length],
  );

  const gps = liveTracker?.gps;
  const gpsAvailable = Boolean(liveTracker?.has_fix && gps?.lat && gps?.lon);

  return (
    <div className="space-y-5">
      <div className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-sm font-semibold uppercase text-brand-forest">123 Mobile Track</p>
          <h1 className="mt-1 text-3xl font-bold text-brand-ink">Operations Dashboard</h1>
        <p className="mt-2 max-w-3xl text-sm leading-6 text-slate-500">
            Track vehicles, assign devices, and keep every install easy to manage.
        </p>
        </div>
        <div className="flex flex-wrap gap-2">
          <Button variant="secondary" onClick={() => setCompanyOpen(true)}>
            <SlidersHorizontal size={16} className="mr-2" />
            Company Details
          </Button>
          <Button onClick={() => setAddVehicleOpen(true)}>
            <Plus size={16} className="mr-2" />
            Add Vehicle
          </Button>
        </div>
      </div>

      <SectionCard className="border-brand-forest/25 bg-emerald-50/70 p-5">
        <div className="flex flex-wrap items-start gap-3">
          <div className="min-w-[180px] flex-1 rounded-md border border-brand-forest/20 bg-white px-4 py-3 shadow-sm">
            <p className="text-xs font-semibold uppercase tracking-wide text-brand-forest">Tracker</p>
            <div className="mt-1 flex items-center gap-2">
              <span className={`h-2.5 w-2.5 rounded-full ${liveError ? "bg-red-500" : "bg-green-600"}`} />
              <h2 className="text-base font-bold text-brand-ink">{liveDevice.id}</h2>
              <span className="text-sm text-slate-500">{liveError ? "Offline" : "Online"}</span>
            </div>
          </div>

          {[
            {
              icon: LocateFixed,
              label: "Location",
              value: gpsAvailable ? "Live" : "Waiting for GPS",
              detail: gpsAvailable ? "Coordinates available" : "Move outdoors to confirm",
            },
            {
              icon: RadioTower,
              label: "Signal",
              value: signalLabel(liveTracker?.cell_rssi),
              detail: "Cellular",
            },
            {
              icon: BatteryMedium,
              label: "Battery",
              value: batteryPercent(liveTracker?.battery_mv),
              detail: "Estimated level",
            },
            {
              icon: ShieldCheck,
              label: "Queue",
              value: String(liveTracker?.queued_messages ?? "--"),
              detail: "Messages waiting",
            },
          ].map((item) => {
            const Icon = item.icon;
            return (
              <div
                key={item.label}
                className="min-w-[120px] flex-1 rounded-md border border-brand-line bg-white px-4 py-3 shadow-sm"
              >
                <div className="flex items-center gap-2">
                  <div className="rounded-md bg-brand-cloud p-2 text-brand-forest">
                    <Icon size={14} />
                  </div>
                  <div className="min-w-0">
                    <p className="text-[11px] font-semibold uppercase text-slate-500">{item.label}</p>
                    <p className="text-sm font-bold text-brand-ink">{item.value}</p>
                    <p className="text-xs text-slate-500">{item.detail}</p>
                  </div>
                </div>
              </div>
            );
          })}

          <div className="min-w-[170px] flex-1 rounded-md border border-brand-line bg-white px-4 py-3 shadow-sm">
            <p className="text-[11px] font-semibold uppercase text-slate-500">Last update</p>
            <p className="mt-1 text-sm font-bold text-brand-ink">{liveTracker?.received_at ?? "No packets yet"}</p>
            <p className="mt-1 text-xs text-slate-500">Most recent update</p>
          </div>
        </div>
      </SectionCard>

      <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-5">
        {kpis.map((stat) => (
          <KPIStatCard key={stat.label} stat={stat} />
        ))}
      </div>

      <div className="grid gap-5 2xl:grid-cols-[minmax(0,1.45fr)_380px]">
        <MapView />
        <SectionCard className="p-5">
          <div className="flex items-center gap-3">
            <div className="rounded-md bg-brand-mint p-3 text-brand-forest">
              <ShieldCheck size={18} />
            </div>
            <div>
              <h2 className="text-base font-semibold text-brand-ink">Getting Started</h2>
              <p className="text-sm text-slate-500">A short checklist for getting the first vehicle online.</p>
            </div>
          </div>
          <div className="mt-5 space-y-3">
            {checklist.map((item) => {
              const Icon = item.done ? CheckCircle2 : CircleDashed;
              return (
                <div key={item.title} className="rounded-md border border-brand-line bg-brand-cloud px-4 py-4">
                  <div className="flex items-start gap-3">
                    <Icon size={18} className={item.done ? "text-brand-forest" : "text-slate-400"} />
                    <div>
                      <p className="font-semibold text-brand-ink">{item.title}</p>
                      <p className="mt-1 text-sm text-slate-500">{item.detail}</p>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        </SectionCard>
      </div>

      <div className="grid gap-5 lg:grid-cols-4">
        {[
          { label: "Reporting profile", value: formatProfileLabel(liveDevice.reportingProfile), icon: Route },
          { label: "Moving interval", value: liveDevice.movingUpdateInterval, icon: RadioTower },
          { label: "Stopped interval", value: liveDevice.stoppedUpdateInterval, icon: RadioTower },
          { label: "Heartbeat asleep", value: liveDevice.heartbeatInterval, icon: RadioTower },
        ].map((item) => {
          const Icon = item.icon;
          return (
            <SectionCard key={item.label} className="p-5">
              <div className="flex items-center gap-3">
                <div className="rounded-md bg-brand-mint p-3 text-brand-forest">
                  <Icon size={18} />
                </div>
                <div>
                  <p className="text-xs font-semibold uppercase text-slate-500">{item.label}</p>
                  <p className="mt-1 text-xl font-bold text-brand-ink">{item.value}</p>
                </div>
              </div>
            </SectionCard>
          );
        })}
      </div>

      <SectionCard className="p-5">
        <h2 className="text-base font-semibold text-brand-ink">Quick actions</h2>
        <div className="mt-4 grid gap-3 md:grid-cols-2 xl:grid-cols-4">
          <Button className="justify-start" onClick={() => setAddVehicleOpen(true)}>
            Add vehicle
          </Button>
          <Link
            href="/devices"
            className="inline-flex h-10 items-center rounded-md border border-brand-line bg-white px-4 text-sm font-semibold text-brand-ink transition hover:bg-brand-cloud"
          >
            Assign tracker
          </Link>
          <Link
            href="/alerts"
            className="inline-flex h-10 items-center rounded-md border border-brand-line bg-white px-4 text-sm font-semibold text-brand-ink transition hover:bg-brand-cloud"
          >
            Review alerts
          </Link>
          <Link
            href="/settings"
            className="inline-flex h-10 items-center rounded-md border border-brand-line bg-white px-4 text-sm font-semibold text-brand-ink transition hover:bg-brand-cloud"
          >
            Edit company details
          </Link>
        </div>
        <p className="mt-3 text-xs text-slate-500">
          GPS confirmation still needs an outdoor test so the tracker can lock onto satellites.
        </p>
      </SectionCard>

      {state.vehicles.length === 0 ? (
        <EmptyState
          title="Start with your first vehicle"
          description="Add the first vehicle and the rest of the account starts to come together."
          actionLabel="Add Vehicle"
          onAction={() => setAddVehicleOpen(true)}
          icon={<MapPinned size={22} />}
        />
      ) : (
        <SectionCard className="p-5">
          <h2 className="text-base font-semibold text-brand-ink">Overview</h2>
          <p className="mt-2 text-sm text-slate-500">
            You now have {state.vehicles.length} vehicle record{state.vehicles.length === 1 ? "" : "s"}
            {hasServiceArea ? ` in ${serviceArea.label}` : ""}. Next up: drivers, alerts, and geofences whenever you want to add them.
          </p>
        </SectionCard>
      )}

      <SetupWorkspaceModal open={companyOpen} onClose={() => setCompanyOpen(false)} />
      <AddVehicleModal open={addVehicleOpen} onClose={() => setAddVehicleOpen(false)} />
    </div>
  );
}
