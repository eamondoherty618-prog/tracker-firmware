"use client";

import { MapPinned, Plus } from "lucide-react";
import { useState } from "react";

import { Button } from "@/components/ui/Button";
import { EmptyState } from "@/components/ui/EmptyState";
import { SectionCard } from "@/components/ui/SectionCard";

export default function GeofencesPage() {
  const [showHint, setShowHint] = useState(false);

  return (
    <div className="space-y-5">
      <div className="flex flex-wrap items-end justify-between gap-3">
        <div>
          <p className="text-sm font-semibold uppercase text-brand-forest">Geofences</p>
          <h1 className="mt-1 text-3xl font-bold text-brand-ink">Geofences</h1>
          <p className="mt-2 text-sm text-slate-500">
            Add arrival, departure, and after-hours zones around the places that matter most.
          </p>
        </div>
        <Button onClick={() => setShowHint((current) => !current)}>
          <Plus size={16} className="mr-2" />
          {showHint ? "Hide Details" : "Create Geofence"}
        </Button>
      </div>

      <SectionCard className="p-5">
        <EmptyState
          title="No geofences configured"
          description="Create the first zone around a depot, yard, customer site, or overnight parking location."
          icon={<MapPinned size={22} />}
        />
      </SectionCard>
      {showHint && (
        <SectionCard className="p-5">
          <div className="flex items-center justify-between gap-3">
            <div>
              <h2 className="text-base font-semibold text-brand-ink">Geofence setup</h2>
              <p className="mt-2 text-sm text-slate-500">
                The next step here is naming a zone and attaching an arrival, departure, or after-hours rule.
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
