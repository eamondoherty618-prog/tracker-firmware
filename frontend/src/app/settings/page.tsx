"use client";

import { useState } from "react";
import { SlidersHorizontal } from "lucide-react";

import { SetupWorkspaceModal } from "@/components/forms/SetupWorkspaceModal";
import { Button } from "@/components/ui/Button";
import { SectionCard } from "@/components/ui/SectionCard";
import { useWorkspace } from "@/lib/workspace";

export default function SettingsPage() {
  const { state, serviceArea, hasServiceArea } = useWorkspace();
  const [companyOpen, setCompanyOpen] = useState(false);

  return (
    <div className="space-y-5">
      <div>
        <p className="text-sm font-semibold uppercase text-brand-forest">Settings</p>
        <h1 className="mt-1 text-3xl font-bold text-brand-ink">Settings</h1>
        <p className="mt-2 text-sm text-slate-500">
          Company details, notifications, and default tracking behavior for this account.
        </p>
      </div>
      <div className="grid gap-5 xl:grid-cols-2">
        <SectionCard className="p-5">
          <h2 className="text-base font-semibold text-brand-ink">Company</h2>
          <div className="mt-4 space-y-3 text-sm">
            <div className="rounded-md border border-brand-line px-4 py-3">
              <p className="text-xs font-semibold uppercase text-slate-500">Company</p>
              <p className="mt-1 font-semibold text-brand-ink">{state.companyName}</p>
            </div>
            <div className="rounded-md border border-brand-line px-4 py-3">
              <p className="text-xs font-semibold uppercase text-slate-500">Service area</p>
              <p className="mt-1 font-semibold text-brand-ink">{hasServiceArea ? serviceArea.label : "Not set"}</p>
            </div>
            <div className="rounded-md border border-brand-line px-4 py-3">
              <p className="text-xs font-semibold uppercase text-slate-500">Primary contact</p>
              <p className="mt-1 font-semibold text-brand-ink">{state.adminName}</p>
              <p className="text-slate-500">{state.adminEmail}</p>
            </div>
          </div>
          <Button className="mt-4" onClick={() => setCompanyOpen(true)}>
            <SlidersHorizontal size={16} className="mr-2" />
            Edit Company Details
          </Button>
        </SectionCard>
        <SectionCard className="p-5">
          <h2 className="text-base font-semibold text-brand-ink">Default tracking behavior</h2>
          <div className="mt-4 grid gap-3">
            {[
              ["Moving update interval", "10 seconds"],
              ["Stopped update interval", "60 seconds"],
              ["Idle timeout", "5 minutes"],
              ["Deep sleep timeout", "15 minutes"],
              ["Heartbeat while asleep", "30 minutes"],
            ].map(([label, value]) => (
              <div key={label} className="rounded-md border border-brand-line px-4 py-3">
                <p className="text-xs font-semibold uppercase text-slate-500">{label}</p>
                <p className="mt-1 font-semibold text-brand-ink">{value}</p>
              </div>
            ))}
          </div>
        </SectionCard>
      </div>
      <SetupWorkspaceModal open={companyOpen} onClose={() => setCompanyOpen(false)} />
    </div>
  );
}
