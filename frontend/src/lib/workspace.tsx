"use client";

import { createContext, ReactNode, useContext, useEffect, useMemo, useState } from "react";

import { Driver, Vehicle } from "@/types";

type DateRangeOption = "Today" | "Last 7 days" | "Last 30 days" | "This month";

export type ServiceAreaOption = {
  id: string;
  label: string;
  center: [number, number];
};

type WorkspaceState = {
  companyName: string;
  serviceAreaId: string;
  timezone: string;
  adminName: string;
  adminEmail: string;
  dateRange: DateRangeOption;
  setupComplete: boolean;
  vehicles: Vehicle[];
  drivers: Driver[];
  trackerAssignmentVehicleId: string | null;
};

type WorkspaceContextValue = {
  state: WorkspaceState;
  serviceArea: ServiceAreaOption;
  hasServiceArea: boolean;
  notifications: { id: string; title: string; detail: string }[];
  setDateRange: (value: DateRangeOption) => void;
  completeSetup: (payload: {
    companyName: string;
    serviceAreaId: string;
    timezone: string;
    adminName: string;
    adminEmail: string;
  }) => void;
  addVehicle: (payload: {
    name: string;
    vehicleNumber: string;
    plate: string;
    make: string;
    model: string;
    year: number;
    type: string;
    notes: string;
    installDate: string;
    enabledFeatures: string[];
  }) => void;
  addDriver: (payload: { name: string; phone: string; license: string; notes: string }) => void;
  assignTrackerToVehicle: (vehicleId: string) => void;
};

const STORAGE_KEY = "mobile-track-workspace";
const unsetServiceArea: ServiceAreaOption = {
  id: "",
  label: "Not set",
  center: [40.7128, -74.006],
};

export const serviceAreaOptions: ServiceAreaOption[] = [
  { id: "north-jersey", label: "North Jersey", center: [40.7357, -74.1724] },
  { id: "essex-county", label: "Essex County", center: [40.7866, -74.2479] },
  { id: "hudson-county", label: "Hudson County", center: [40.7449, -74.0288] },
  { id: "bergen-county", label: "Bergen County", center: [40.9263, -74.0770] },
  { id: "nyc-metro", label: "NYC Metro", center: [40.7128, -74.006] },
];

const defaultState: WorkspaceState = {
  companyName: "123 Mobile Track",
  serviceAreaId: "",
  timezone: "America/New_York",
  adminName: "Eamon Doherty",
  adminEmail: "ops@123mobiletrack.com",
  dateRange: "Last 7 days",
  setupComplete: false,
  vehicles: [],
  drivers: [],
  trackerAssignmentVehicleId: null,
};

const WorkspaceContext = createContext<WorkspaceContextValue | null>(null);

function offsetPoint(center: [number, number], index: number): [number, number] {
  const latOffset = (index % 3) * 0.012 - 0.012;
  const lngOffset = Math.floor(index / 3) * 0.014 - 0.014;
  return [center[0] + latOffset, center[1] + lngOffset];
}

export function WorkspaceProvider({ children }: { children: ReactNode }) {
  const [state, setState] = useState<WorkspaceState>(defaultState);

  useEffect(() => {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    if (!raw) return;

    try {
      const parsed = JSON.parse(raw) as WorkspaceState;
      const normalized = { ...defaultState, ...parsed };

      // Clear legacy demo-region defaults from older first-run sessions.
      if (!normalized.setupComplete && normalized.serviceAreaId) {
        normalized.serviceAreaId = "";
      }

      setState(normalized);
    } catch {
      window.localStorage.removeItem(STORAGE_KEY);
    }
  }, []);

  useEffect(() => {
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
  }, [state]);

  const serviceArea =
    serviceAreaOptions.find((option) => option.id === state.serviceAreaId) ?? unsetServiceArea;
  const hasServiceArea = Boolean(state.serviceAreaId);

  const notifications = useMemo(() => {
    const items = [
      !state.setupComplete && {
        id: "setup",
        title: "Finish company setup",
        detail: "Add your company name, service area, and contact details.",
      },
      state.vehicles.length === 0 && {
        id: "vehicle",
        title: "Add your first vehicle",
        detail: "Assign tracker-001 to a vehicle so trips, alerts, and maintenance can start cleanly.",
      },
      state.drivers.length === 0 && {
        id: "driver",
        title: "Drivers are still optional",
        detail: "You can add drivers now or leave this for later once the first install is mounted.",
      },
    ].filter(Boolean);

    return items as { id: string; title: string; detail: string }[];
  }, [state.drivers.length, state.setupComplete, state.vehicles.length]);

  const value = useMemo<WorkspaceContextValue>(
    () => ({
      state,
      serviceArea,
      hasServiceArea,
      notifications,
      setDateRange: (value) => setState((current) => ({ ...current, dateRange: value })),
      completeSetup: (payload) =>
        setState((current) => ({
          ...current,
          ...payload,
          setupComplete: true,
        })),
      addVehicle: (payload) =>
        setState((current) => {
          const nextIndex = current.vehicles.length + 1;
          const location = offsetPoint(
            hasServiceArea ? serviceArea.center : unsetServiceArea.center,
            nextIndex,
          );
          const vehicle: Vehicle = {
            id: `VH-${String(1000 + nextIndex)}`,
            name: payload.name,
            vehicleNumber: payload.vehicleNumber,
            plate: payload.plate,
            vin: "VIN pending",
            make: payload.make,
            model: payload.model,
            year: payload.year,
            type: payload.type,
            assignedDriver: "Not assigned",
            region: hasServiceArea ? serviceArea.label : "Not set",
            status: "parked",
            gpsOnline: false,
            deviceStatus: current.trackerAssignmentVehicleId ? "offline" : "online",
            enabledFeatures:
              payload.enabledFeatures.length > 0
                ? payload.enabledFeatures
                : ["GPS Tracking", "Live Location", "Motion Wake", "Smart Sleep Mode"],
            batteryLevel: 100,
            lastSeen: "Not assigned yet",
            notes: payload.notes || "Ready to be assigned and installed.",
            installDate: payload.installDate || "Install date not set",
            hardwareType: "ESP32 + SIM Tracker",
            deviceAssignment:
              current.vehicles.length === 0 && !current.trackerAssignmentVehicleId ? "tracker-001" : "Not assigned",
            location: { lat: location[0], lng: location[1], label: payload.name },
          };

          return {
            ...current,
            setupComplete: true,
            vehicles: [...current.vehicles, vehicle],
            trackerAssignmentVehicleId:
              current.vehicles.length === 0 && !current.trackerAssignmentVehicleId ? vehicle.id : current.trackerAssignmentVehicleId,
          };
        }),
      addDriver: (payload) =>
        setState((current) => ({
          ...current,
          drivers: [
            ...current.drivers,
            {
              id: `DRV-${String(current.drivers.length + 1).padStart(2, "0")}`,
              name: payload.name,
              phone: payload.phone,
              assignedVehicle: "Not assigned",
              status: "available",
              region: hasServiceArea ? serviceArea.label : "Not set",
              license: payload.license || "License not added",
              notes: payload.notes || "Added from company details.",
            },
          ],
        })),
      assignTrackerToVehicle: (vehicleId) =>
        setState((current) => ({
          ...current,
          trackerAssignmentVehicleId: vehicleId,
          vehicles: current.vehicles.map((vehicle) => ({
            ...vehicle,
            deviceAssignment: vehicle.id === vehicleId ? "tracker-001" : vehicle.deviceAssignment,
            gpsOnline: vehicle.id === vehicleId,
            deviceStatus: vehicle.id === vehicleId ? "online" : vehicle.deviceStatus,
            lastSeen: vehicle.id === vehicleId ? "Connected now" : vehicle.lastSeen,
          })),
        })),
    }),
    [hasServiceArea, notifications, serviceArea, state],
  );

  return <WorkspaceContext.Provider value={value}>{children}</WorkspaceContext.Provider>;
}

export function useWorkspace() {
  const context = useContext(WorkspaceContext);
  if (!context) {
    throw new Error("useWorkspace must be used inside WorkspaceProvider");
  }
  return context;
}
