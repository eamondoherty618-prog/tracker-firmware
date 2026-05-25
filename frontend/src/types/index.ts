export type VehicleStatus = "moving" | "idle" | "parked" | "offline";
export type DeviceSleepState = "awake" | "idle-watch" | "deep-sleep";
export type DeviceMotionState = "moving" | "stopped" | "vibration";
export type DeviceReportingProfile = "balanced" | "smart-sleep" | "high-frequency";
export type AlertSeverity = "critical" | "warning" | "info";
export type FeatureCategory = "active" | "beta" | "comingSoon" | "hardware";

export interface LocationPoint {
  lat: number;
  lng: number;
  label: string;
}

export interface Vehicle {
  id: string;
  name: string;
  vehicleNumber: string;
  plate: string;
  vin: string;
  make: string;
  model: string;
  year: number;
  type: string;
  assignedDriver: string;
  region: string;
  status: VehicleStatus;
  gpsOnline: boolean;
  deviceStatus: "online" | "offline" | "needs-attention";
  enabledFeatures: string[];
  batteryLevel: number;
  lastSeen: string;
  notes: string;
  installDate: string;
  hardwareType: string;
  deviceAssignment: string;
  location: LocationPoint;
}

export interface Device {
  id: string;
  assignedVehicleId: string;
  simNumber: string;
  firmwareVersion: string;
  batteryLevel: number;
  signalStrength: number;
  gpsLock: boolean;
  sleepState: DeviceSleepState;
  motionState: DeviceMotionState;
  movingUpdateInterval: string;
  stoppedUpdateInterval: string;
  idleTimeout: string;
  deepSleepTimeout: string;
  heartbeatInterval: string;
  reportingProfile: DeviceReportingProfile;
  lastPing: string;
  online: boolean;
  enabledFeatures: string[];
}

export interface Driver {
  id: string;
  name: string;
  phone: string;
  assignedVehicle: string;
  status: "on-route" | "available" | "off-duty";
  region: string;
  license: string;
  notes: string;
}

export interface Trip {
  id: string;
  vehicle: string;
  driver: string;
  startTime: string;
  endTime: string;
  distance: string;
  duration: string;
  region: string;
  status: "completed" | "active" | "review";
}

export interface FleetAlert {
  id: string;
  title: string;
  vehicle: string;
  type: string;
  severity: AlertSeverity;
  createdAt: string;
  detail: string;
}

export interface Geofence {
  id: string;
  name: string;
  region: string;
  vehiclesInside: number;
  rule: string;
}

export interface MaintenanceRecord {
  id: string;
  vehicle: string;
  service: string;
  dueMileage: string;
  dueDate: string;
  status: "scheduled" | "due-soon" | "overdue";
  notes: string;
}

export interface FeatureModule {
  id: string;
  name: string;
  description: string;
  category: FeatureCategory;
  statusLabel: string;
}

export interface KPIStat {
  label: string;
  value: string;
  delta: string;
  tone: "green" | "navy" | "amber" | "red";
}
