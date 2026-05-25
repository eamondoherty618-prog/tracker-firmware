"use client";

import { useState } from "react";
import { BellRing } from "lucide-react";

import { Button } from "@/components/ui/Button";
import { EmptyState } from "@/components/ui/EmptyState";
import { SectionCard } from "@/components/ui/SectionCard";

export default function AlertsPage() {
  const [showHint, setShowHint] = useState(false);

  return (
    <div className="space-y-5">
      <div>
        <p className="text-sm font-semibold uppercase text-brand-forest">Alerts</p>
        <h1 className="mt-1 text-3xl font-bold text-brand-ink">Alerts</h1>
        <p className="mt-2 text-sm text-slate-500">
          Operational alert feed for speeding, offline vehicles, geofences, and low battery events.
        </p>
      </div>
      <EmptyState
        title="No alerts"
        description="Alerts appear after the tracker is assigned and rules like speeding, idle, offline, or geofence events are turned on."
        icon={<BellRing size={22} />}
        actionLabel="Alert setup"
        onAction={() => setShowHint((current) => !current)}
      />
      {showHint && (
        <SectionCard className="p-5">
          <div className="flex items-center justify-between gap-3">
            <div>
              <h2 className="text-base font-semibold text-brand-ink">Alerts appear automatically</h2>
              <p className="mt-2 text-sm text-slate-500">
                Start with offline, idle, and speeding alerts, then add geofences once the first vehicle is fully configured.
              </p>
            </div>
            <Button variant="secondary" onClick={() => setShowHint(false)}>
              Close
            </Button>
          </div>
        </SectionCard>
      )}
    </div>
  );
}
