import { alerts } from "@/data/mockData";

import { Badge } from "./Badge";
import { SectionCard } from "./SectionCard";

export function AlertsPanel() {
  return (
    <SectionCard className="p-5">
      <div className="flex items-center justify-between">
        <div>
          <h3 className="text-base font-semibold text-brand-ink">Recent Alerts</h3>
          <p className="text-sm text-slate-500">Prioritized for dispatch and operations review</p>
        </div>
        <Badge tone="warning">17 open</Badge>
      </div>
      <div className="mt-5 space-y-3">
        {alerts.map((alert) => (
          <div key={alert.id} className="rounded-md border border-brand-line px-4 py-3">
            <div className="flex items-start justify-between gap-4">
              <div>
                <p className="text-sm font-semibold text-brand-ink">{alert.title}</p>
                <p className="mt-1 text-sm text-slate-500">
                  {alert.vehicle} · {alert.type}
                </p>
                <p className="mt-2 text-sm text-brand-text">{alert.detail}</p>
              </div>
              <div className="text-right">
                <Badge tone={alert.severity}>{alert.severity}</Badge>
                <p className="mt-2 text-xs text-slate-400">{alert.createdAt}</p>
              </div>
            </div>
          </div>
        ))}
      </div>
    </SectionCard>
  );
}
