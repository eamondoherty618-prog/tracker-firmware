"use client";

import dynamic from "next/dynamic";

const LiveFleetMap = dynamic(
  () => import("./LiveFleetMapClient").then((module) => module.LiveFleetMapClient),
  {
    ssr: false,
    loading: () => (
      <div className="flex h-[460px] items-center justify-center rounded-lg border border-brand-line bg-white text-sm text-slate-500 shadow-soft">
        Loading live map tiles...
      </div>
    ),
  },
);

export function MapView({
  selectedVehicleId,
  onSelectVehicle,
}: {
  selectedVehicleId?: string;
  onSelectVehicle?: (vehicleId: string) => void;
}) {
  return <LiveFleetMap selectedVehicleId={selectedVehicleId} onSelectVehicle={onSelectVehicle} />;
}
