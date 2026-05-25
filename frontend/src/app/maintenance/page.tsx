"use client";

import { useState } from "react";
import { Wrench } from "lucide-react";

import { Button } from "@/components/ui/Button";
import { EmptyState } from "@/components/ui/EmptyState";
import { SectionCard } from "@/components/ui/SectionCard";

export default function MaintenancePage() {
  const [showHint, setShowHint] = useState(false);

  return (
    <div className="space-y-5">
      <div>
        <p className="text-sm font-semibold uppercase text-brand-forest">Maintenance</p>
        <h1 className="mt-1 text-3xl font-bold text-brand-ink">Maintenance</h1>
        <p className="mt-2 text-sm text-slate-500">
          Track scheduled service work, due mileage, inspection windows, and maintenance reminders.
        </p>
      </div>
      <EmptyState
        title="No maintenance records"
        description="Service history starts once your first vehicle is in the system. Oil changes, tires, brakes, inspections, and custom reminders can all start here."
        icon={<Wrench size={22} />}
        actionLabel="Service setup"
        onAction={() => setShowHint((current) => !current)}
      />
      {showHint && (
        <SectionCard className="p-5">
          <div className="flex items-center justify-between gap-3">
            <div>
              <h2 className="text-base font-semibold text-brand-ink">Service reminders start here</h2>
              <p className="mt-2 text-sm text-slate-500">
                Start with oil change, tires, brakes, and inspection schedules for the first tracked vehicle.
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
