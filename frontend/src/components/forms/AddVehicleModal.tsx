"use client";

import { useState } from "react";
import { X } from "lucide-react";

import { featureCatalog } from "@/data/mockData";
import { useWorkspace } from "@/lib/workspace";

import { Button } from "../ui/Button";

const vehicleTypes = ["Van", "Service Truck", "Car", "SUV", "Tow Truck", "Motorcycle", "Trailer", "Other"];

export function AddVehicleModal({
  open,
  onClose,
}: {
  open: boolean;
  onClose: () => void;
}) {
  const { addVehicle, serviceArea, hasServiceArea } = useWorkspace();
  const [form, setForm] = useState({
    name: "",
    vehicleNumber: "",
    plate: "",
    make: "",
    model: "",
    year: "2026",
    type: vehicleTypes[0],
    notes: "",
    installDate: "",
  });
  const [selectedFeatures, setSelectedFeatures] = useState<string[]>([
    "GPS Tracking",
    "Live Location",
    "Motion Wake",
    "Smart Sleep Mode",
  ]);

  function toggleFeature(feature: string) {
    setSelectedFeatures((current) =>
      current.includes(feature) ? current.filter((item) => item !== feature) : [...current, feature],
    );
  }

  if (!open) return null;

  const canSubmit = form.name.trim().length > 0;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-brand-ink/45 px-4">
      <div className="max-h-[92vh] w-full max-w-4xl overflow-y-auto rounded-lg bg-white shadow-panel">
        <div className="sticky top-0 flex items-center justify-between border-b border-brand-line bg-white px-6 py-4">
          <div>
            <h3 className="text-lg font-semibold text-brand-ink">Add Vehicle</h3>
            <p className="text-sm text-slate-500">Create a vehicle record and link tracking hardware when ready.</p>
          </div>
          <button onClick={onClose} className="rounded-md border border-brand-line p-2">
            <X size={18} />
          </button>
        </div>

        <div className="grid gap-4 p-6 md:grid-cols-2">
          {[
            ["Vehicle name", "name"],
            ["Vehicle number", "vehicleNumber"],
            ["Plate number", "plate"],
            ["Make", "make"],
            ["Model", "model"],
            ["Year", "year"],
          ].map(([label, key]) => (
            <label key={label} className="text-sm font-medium text-brand-text">
              {label}
              <input
                className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
                placeholder={label}
                value={form[key as keyof typeof form]}
                onChange={(event) => setForm((current) => ({ ...current, [key]: event.target.value }))}
              />
            </label>
          ))}

          <label className="text-sm font-medium text-brand-text">
            Vehicle type
            <select
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              value={form.type}
              onChange={(event) => setForm((current) => ({ ...current, type: event.target.value }))}
            >
              {vehicleTypes.map((type) => (
                <option key={type} value={type}>
                  {type}
                </option>
              ))}
            </select>
          </label>

          <label className="text-sm font-medium text-brand-text">
            Primary service area
            <input
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              value={hasServiceArea ? serviceArea.label : "Set in Company Details"}
              disabled
            />
          </label>

          <label className="text-sm font-medium text-brand-text">
            Planned install date
            <input
              className="mt-2 h-11 w-full rounded-md border border-brand-line px-3"
              type="date"
              value={form.installDate}
              onChange={(event) => setForm((current) => ({ ...current, installDate: event.target.value }))}
            />
          </label>

          <label className="md:col-span-2 text-sm font-medium text-brand-text">
            Notes
            <textarea
              rows={4}
              className="mt-2 w-full rounded-md border border-brand-line px-3 py-3"
              placeholder="Install notes, mounting details, parking habits, future service items..."
              value={form.notes}
              onChange={(event) => setForm((current) => ({ ...current, notes: event.target.value }))}
            />
          </label>

          <div className="md:col-span-2">
            <p className="text-sm font-medium text-brand-text">Features enabled</p>
            <div className="mt-3 grid gap-2 sm:grid-cols-2 lg:grid-cols-3">
              {featureCatalog.map((feature) => (
                <label key={feature} className="flex items-center gap-2 rounded-md border border-brand-line px-3 py-2 text-sm">
                  <input
                    type="checkbox"
                    className="h-4 w-4 rounded border-brand-line"
                    checked={selectedFeatures.includes(feature)}
                    onChange={() => toggleFeature(feature)}
                  />
                  <span>{feature}</span>
                </label>
              ))}
            </div>
            <p className="mt-3 text-xs text-slate-500">
              These features are saved with the vehicle profile and can be refined later.
            </p>
          </div>
        </div>

        <div className="flex justify-end gap-3 border-t border-brand-line px-6 py-4">
          <Button variant="secondary" onClick={onClose}>
            Cancel
          </Button>
          <Button
            disabled={!canSubmit}
            onClick={() => {
              addVehicle({
                name: form.name,
                vehicleNumber: form.vehicleNumber || "01",
                plate: form.plate || "Plate not added",
                make: form.make || "Make not added",
                model: form.model || "Model not added",
                year: Number(form.year) || new Date().getFullYear(),
                type: form.type,
                notes: form.notes,
                installDate: form.installDate,
                enabledFeatures: selectedFeatures,
              });
              onClose();
              setForm({
                name: "",
                vehicleNumber: "",
                plate: "",
                make: "",
                model: "",
                year: "2026",
                type: vehicleTypes[0],
                notes: "",
                installDate: "",
              });
              setSelectedFeatures(["GPS Tracking", "Live Location", "Motion Wake", "Smart Sleep Mode"]);
            }}
          >
            Create Vehicle
          </Button>
        </div>
        {!canSubmit && <p className="px-6 pb-5 text-sm text-slate-500">Add a vehicle name to create the record.</p>}
      </div>
    </div>
  );
}
