"use client";

import { useState } from "react";
import { X } from "lucide-react";

import { serviceAreaOptions, useWorkspace } from "@/lib/workspace";

import { Button } from "../ui/Button";

export function SetupWorkspaceModal({
  open,
  onClose,
}: {
  open: boolean;
  onClose: () => void;
}) {
  const { state, completeSetup } = useWorkspace();
  const [form, setForm] = useState({
    companyName: state.companyName,
    serviceAreaId: state.serviceAreaId,
    timezone: state.timezone,
    adminName: state.adminName,
    adminEmail: state.adminEmail,
  });

  if (!open) return null;

  const canSubmit =
    form.companyName.trim().length > 0 &&
    form.adminName.trim().length > 0 &&
    form.adminEmail.trim().length > 0;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-brand-ink/45 px-4">
      <div className="w-full max-w-2xl rounded-lg bg-white shadow-panel">
        <div className="flex items-center justify-between border-b border-brand-line px-6 py-4">
          <div>
            <h3 className="text-lg font-semibold text-brand-ink">Company Details</h3>
            <p className="text-sm text-slate-500">Set the company name, service area, and primary contact.</p>
          </div>
          <button onClick={onClose} className="rounded-md border border-brand-line p-2">
            <X size={18} />
          </button>
        </div>

        <div className="grid gap-4 p-6 md:grid-cols-2">
          <label className="text-sm font-medium text-brand-text">
            Company name
            <input
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              value={form.companyName}
              onChange={(event) => setForm((current) => ({ ...current, companyName: event.target.value }))}
            />
          </label>
          <label className="text-sm font-medium text-brand-text">
            Service area
            <select
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              value={form.serviceAreaId}
              onChange={(event) => setForm((current) => ({ ...current, serviceAreaId: event.target.value }))}
            >
              <option value="">Choose service area</option>
              {serviceAreaOptions.map((option) => (
                <option key={option.id} value={option.id}>
                  {option.label}
                </option>
              ))}
            </select>
          </label>
          <label className="text-sm font-medium text-brand-text">
            Timezone
            <input
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              value={form.timezone}
              onChange={(event) => setForm((current) => ({ ...current, timezone: event.target.value }))}
            />
          </label>
          <label className="text-sm font-medium text-brand-text">
            Primary contact
            <input
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              value={form.adminName}
              onChange={(event) => setForm((current) => ({ ...current, adminName: event.target.value }))}
            />
          </label>
          <label className="md:col-span-2 text-sm font-medium text-brand-text">
            Contact email
            <input
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              value={form.adminEmail}
              onChange={(event) => setForm((current) => ({ ...current, adminEmail: event.target.value }))}
            />
          </label>
        </div>

        <div className="flex justify-end gap-3 border-t border-brand-line px-6 py-4">
          <Button variant="secondary" onClick={onClose}>
            Close
          </Button>
          <Button
            disabled={!canSubmit}
            onClick={() => {
              completeSetup(form);
              onClose();
            }}
          >
            Save Changes
          </Button>
        </div>
        {!canSubmit && (
          <p className="px-6 pb-5 text-sm text-slate-500">Add a company name, contact name, and contact email to save changes.</p>
        )}
      </div>
    </div>
  );
}
