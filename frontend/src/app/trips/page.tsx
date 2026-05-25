"use client";

import { useState } from "react";
import { Route } from "lucide-react";

import { Button } from "@/components/ui/Button";
import { EmptyState } from "@/components/ui/EmptyState";
import { SectionCard } from "@/components/ui/SectionCard";

export default function TripsPage() {
  const [showHint, setShowHint] = useState(false);

  return (
    <div className="space-y-5">
      <div>
        <p className="text-sm font-semibold uppercase text-brand-forest">Trips</p>
        <h1 className="mt-1 text-3xl font-bold text-brand-ink">Trips</h1>
        <p className="mt-2 text-sm text-slate-500">
          Review trip history, mileage, and route timing once vehicles are actively reporting.
        </p>
      </div>
      <EmptyState
        title="No trips recorded"
        description="Trip history will appear after a vehicle is assigned and the tracker has a usable GPS fix."
        icon={<Route size={22} />}
        actionLabel="Trip requirements"
        onAction={() => setShowHint((current) => !current)}
      />
      {showHint && (
        <SectionCard className="p-5">
          <div className="flex items-center justify-between gap-3">
            <div>
              <h2 className="text-base font-semibold text-brand-ink">Trips appear automatically</h2>
              <p className="mt-2 text-sm text-slate-500">
                Trips begin automatically once a vehicle is assigned, the tracker has a GPS fix, and movement is detected.
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
