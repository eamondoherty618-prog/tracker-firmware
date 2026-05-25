# 123 Mobile Track Frontend

Production-style telematics and fleet tracking SaaS frontend built with Next.js, React, Tailwind CSS, and TypeScript.

## Run Notes

This workspace did not have `node` or `npm` installed when the app was generated, so the project source is complete but dependencies were not installed locally yet.

Once Node is available:

```bash
cd frontend
npm install
npm run dev
```

## Recommended Folder Structure

```text
frontend/
  public/
    123-mobile-track-logo.png
  src/
    app/
      dashboard/
      vehicles/
      devices/
      drivers/
      trips/
      alerts/
      geofences/
      maintenance/
      features/
      settings/
      globals.css
      layout.tsx
      page.tsx
    components/
      branding/
      drawers/
      filters/
      forms/
      layout/
      map/
      tables/
      ui/
    data/
      mockData.ts
    lib/
      navigation.ts
      utils.ts
    types/
      index.ts
```

## Reusable Components

- `AppShell`
- `SidebarNav`
- `TopHeader`
- `StickyFilterPanel`
- `MobileFilterDrawer`
- `BrandLogo`
- `KPIStatCard`
- `MapView`
- `VehicleTable`
- `VehicleDetailDrawer`
- `DeviceTable`
- `DeviceDetailDrawer`
- `DriversTable`
- `TripsTable`
- `FeatureBadgeList`
- `AddVehicleModal`
- `AlertsPanel`
- `SettingsPanel`
- `EmptyState`
- `ComingSoonCard`
- `TrackingProfileCard`
- `Button`
- `Badge`
- `SectionCard`

## Mock Data Files

- `src/data/mockData.ts`
  - vehicles
  - devices
  - drivers
  - trips
  - alerts
  - geofences
  - maintenance records
  - feature modules
  - KPI stats

## Backend Integration TODO Roadmap

1. Add auth and organization-aware routing.
2. Replace mock data with typed API clients and database-backed endpoints.
3. Add websocket or SSE streams for live vehicle and device updates.
4. Connect tracker/device configuration forms to real device APIs.
5. Add billing, entitlements, audit logs, and role-based access control.
6. Add alert rules, saved views, exports, and reporting workflows.

## Real Map and Telemetry Swap-In Notes

- Replace `src/components/map/MapView.tsx` with a real map provider such as Mapbox, Google Maps, or MapLibre.
- Feed map markers, routes, and geofences from backend telemetry instead of `mockData.ts`.
- Bind `VehicleDetailDrawer` and `DeviceDetailDrawer` to live data and timeline queries.
- Wire reporting profile edits to the eventual tracker management API for ESP32 + SIM devices.
- Add a socket stream layer for live movement, alert transitions, and online/offline state changes.
