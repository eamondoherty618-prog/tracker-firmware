import {
  BellRing,
  CarFront,
  Gauge,
  MapPinned,
  RadioTower,
  Route,
  Settings,
  ShieldAlert,
  Sparkles,
  Users,
  Wrench,
} from "lucide-react";

export const navigationItems = [
  { label: "Dashboard", href: "/dashboard", icon: Gauge },
  { label: "Vehicles", href: "/vehicles", icon: CarFront },
  { label: "Devices", href: "/devices", icon: RadioTower },
  { label: "Drivers", href: "/drivers", icon: Users },
  { label: "Trips", href: "/trips", icon: Route },
  { label: "Alerts", href: "/alerts", icon: ShieldAlert },
  { label: "Geofences", href: "/geofences", icon: MapPinned },
  { label: "Maintenance", href: "/maintenance", icon: Wrench },
  { label: "Features", href: "/features", icon: Sparkles },
  { label: "Settings", href: "/settings", icon: Settings },
];

export const quickActions = [
  { label: "New alert rule", icon: BellRing },
  { label: "Assign tracker", icon: RadioTower },
];
