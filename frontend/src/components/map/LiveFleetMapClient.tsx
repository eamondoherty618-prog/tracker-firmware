"use client";

import "leaflet/dist/leaflet.css";

import { divIcon } from "leaflet";
import { Layers3, LocateFixed, Route } from "lucide-react";
import { MapContainer, Marker, Polyline, TileLayer, Tooltip, ZoomControl } from "react-leaflet";

import { useLiveTracker } from "@/lib/liveTracker";
import { useWorkspace } from "@/lib/workspace";

import { Badge } from "../ui/Badge";
import { SectionCard } from "../ui/SectionCard";

function markerIcon(selected: boolean) {
  return divIcon({
    className: "",
    html: `<div style="
      width: 18px;
      height: 18px;
      border-radius: 999px;
      background: ${selected ? "#15803d" : "#173754"};
      border: 3px solid white;
      box-shadow: 0 10px 24px rgba(18, 38, 63, 0.22);
    "></div>`,
    iconSize: [18, 18],
    iconAnchor: [9, 9],
  });
}

export function LiveFleetMapClient({
  selectedVehicleId,
  onSelectVehicle,
}: {
  selectedVehicleId?: string;
  onSelectVehicle?: (vehicleId: string) => void;
}) {
  const { state, serviceArea, hasServiceArea } = useWorkspace();
  const { liveTracker } = useLiveTracker();
  const hasGps = Boolean(liveTracker?.has_fix && liveTracker.gps?.lat && liveTracker.gps?.lon);
  const livePoint: [number, number] | null = hasGps
    ? [Number(liveTracker?.gps?.lat), Number(liveTracker?.gps?.lon)]
    : null;
  const markers =
    state.vehicles.length > 0
      ? state.vehicles.map((vehicle) => ({
          id: vehicle.id,
          name: vehicle.name,
          region: vehicle.region,
          point:
            vehicle.id === state.trackerAssignmentVehicleId && livePoint
              ? livePoint
              : ([vehicle.location.lat, vehicle.location.lng] as [number, number]),
        }))
      : [
          {
            id: "tracker-001",
            name: "tracker-001",
            region: hasServiceArea ? serviceArea.label : "Live tracker",
            point: livePoint ?? serviceArea.center,
          },
        ];
  const center = markers[0]?.point ?? serviceArea.center;
  const path = markers.slice(0, 4).map((marker) => marker.point);

  return (
    <SectionCard className="overflow-hidden">
      <div className="flex items-center justify-between border-b border-brand-line px-5 py-4">
        <div>
          <h2 className="text-lg font-semibold text-brand-ink">Live map</h2>
          <p className="text-sm text-slate-500">
            Current tracker location and vehicle status.
          </p>
        </div>
        <div className="flex items-center gap-2 text-xs">
          <Badge>OpenStreetMap</Badge>
          <Badge>Vehicle markers</Badge>
          <Badge>Route history</Badge>
        </div>
      </div>

      <div className="relative min-h-[460px]">
        <MapContainer
          center={center}
          zoom={9}
          zoomControl={false}
          className="h-[460px] w-full"
          scrollWheelZoom
        >
          <ZoomControl position="bottomright" />
          <TileLayer
            attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
            url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
          />

          {path.length > 1 && <Polyline positions={path} pathOptions={{ color: "#15803d", weight: 4, opacity: 0.65, dashArray: "10 8" }} />}

          {markers.map((vehicle) => (
            <Marker
              key={vehicle.id}
              position={vehicle.point}
              icon={markerIcon(selectedVehicleId === vehicle.id)}
              eventHandlers={{
                click: () => onSelectVehicle?.(vehicle.id),
              }}
            >
              <Tooltip direction="top" offset={[0, -10]} opacity={1}>
                <div className="text-xs">
                  <p className="font-semibold">{vehicle.name}</p>
                  <p>{vehicle.region}</p>
                </div>
              </Tooltip>
            </Marker>
          ))}
        </MapContainer>

        <div className="pointer-events-none absolute bottom-4 left-4 right-4 flex flex-wrap items-center justify-between gap-3 rounded-lg border border-white/70 bg-white/92 px-4 py-3 shadow-panel backdrop-blur">
          <div className="flex items-center gap-2 text-sm text-brand-text">
            <Layers3 size={16} className="text-brand-navy" />
            {state.vehicles.length > 0 ? "Fleet view" : hasServiceArea ? "Service area view" : "Tracker view"}
          </div>
          <div className="flex items-center gap-3 text-xs text-slate-500">
            <span className="inline-flex items-center gap-1">
              <span className="h-2.5 w-2.5 rounded-full bg-brand-forest" />
              Selected
            </span>
            <span className="inline-flex items-center gap-1">
              <span className="h-2.5 w-2.5 rounded-full bg-brand-navy" />
              Vehicle
            </span>
            <span className="inline-flex items-center gap-1">
              <Route size={14} />
              Route history
            </span>
            <span className="inline-flex items-center gap-1">
              <LocateFixed size={14} />
              {hasGps ? "Live GPS" : hasServiceArea ? "Area view" : "Waiting for GPS"}
            </span>
          </div>
        </div>
      </div>
    </SectionCard>
  );
}
