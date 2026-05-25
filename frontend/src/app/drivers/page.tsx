"use client";

import { UserRound } from "lucide-react";
import { useState } from "react";

import { AddDriverModal } from "@/components/forms/AddDriverModal";
import { Button } from "@/components/ui/Button";
import { EmptyState } from "@/components/ui/EmptyState";
import { SectionCard } from "@/components/ui/SectionCard";
import { useWorkspace } from "@/lib/workspace";

export default function DriversPage() {
  const { state } = useWorkspace();
  const [modalOpen, setModalOpen] = useState(false);

  return (
    <div className="space-y-5">
      <div>
        <p className="text-sm font-semibold uppercase text-brand-forest">Drivers</p>
        <h1 className="mt-1 text-3xl font-bold text-brand-ink">Drivers</h1>
        <p className="mt-2 text-sm text-slate-500">
          Add operators, contact details, and region assignments as the fleet grows.
        </p>
      </div>
      {state.drivers.length === 0 ? (
        <EmptyState
          title="No drivers added"
          description="Add drivers whenever you want operator names, contact details, and regional assignments in the account."
          icon={<UserRound size={22} />}
          actionLabel="Add Driver"
          onAction={() => setModalOpen(true)}
        />
      ) : (
        <SectionCard className="overflow-hidden">
          <div className="flex items-center justify-between border-b border-brand-line px-5 py-4">
            <div>
              <h2 className="text-base font-semibold text-brand-ink">Drivers</h2>
              <p className="text-sm text-slate-500">Drivers in this account</p>
            </div>
            <Button onClick={() => setModalOpen(true)}>Add Driver</Button>
          </div>
          <div className="grid gap-3 p-5 md:grid-cols-2">
            {state.drivers.map((driver) => (
              <div key={driver.id} className="rounded-md border border-brand-line bg-brand-cloud px-4 py-4">
                <p className="font-semibold text-brand-ink">{driver.name}</p>
                <p className="mt-1 text-sm text-slate-500">{driver.phone}</p>
                <p className="mt-3 text-sm text-brand-text">License: {driver.license}</p>
                <p className="mt-1 text-sm text-brand-text">Region: {driver.region}</p>
              </div>
            ))}
          </div>
        </SectionCard>
      )}
      <AddDriverModal open={modalOpen} onClose={() => setModalOpen(false)} />
    </div>
  );
}
