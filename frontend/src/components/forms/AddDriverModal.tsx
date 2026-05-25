"use client";

import { useState } from "react";
import { X } from "lucide-react";

import { useWorkspace } from "@/lib/workspace";

import { Button } from "../ui/Button";

export function AddDriverModal({
  open,
  onClose,
}: {
  open: boolean;
  onClose: () => void;
}) {
  const { addDriver } = useWorkspace();
  const [form, setForm] = useState({
    name: "",
    phone: "",
    license: "",
    notes: "",
  });

  if (!open) return null;

  const canSubmit = form.name.trim().length > 0;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-brand-ink/45 px-4">
      <div className="w-full max-w-xl rounded-lg bg-white shadow-panel">
        <div className="flex items-center justify-between border-b border-brand-line px-6 py-4">
          <div>
            <h3 className="text-lg font-semibold text-brand-ink">Add Driver</h3>
            <p className="text-sm text-slate-500">Create a real operator record for this account.</p>
          </div>
          <button onClick={onClose} className="rounded-md border border-brand-line p-2">
            <X size={18} />
          </button>
        </div>
        <div className="grid gap-4 p-6">
          {[
            ["Driver name", "name"],
            ["Phone number", "phone"],
            ["License number", "license"],
          ].map(([label, key]) => (
            <label key={key} className="text-sm font-medium text-brand-text">
              {label}
              <input
                className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
                value={form[key as keyof typeof form]}
                onChange={(event) => setForm((current) => ({ ...current, [key]: event.target.value }))}
              />
            </label>
          ))}
          <label className="text-sm font-medium text-brand-text">
            Notes
            <textarea
              rows={4}
              className="mt-2 w-full rounded-md border border-brand-line px-3 py-3"
              value={form.notes}
              onChange={(event) => setForm((current) => ({ ...current, notes: event.target.value }))}
            />
          </label>
        </div>
        <div className="flex justify-end gap-3 border-t border-brand-line px-6 py-4">
          <Button variant="secondary" onClick={onClose}>
            Cancel
          </Button>
          <Button
            disabled={!canSubmit}
            onClick={() => {
              addDriver(form);
              onClose();
              setForm({ name: "", phone: "", license: "", notes: "" });
            }}
          >
            Save Driver
          </Button>
        </div>
        {!canSubmit && <p className="px-6 pb-5 text-sm text-slate-500">Add a driver name to save the record.</p>}
      </div>
    </div>
  );
}
