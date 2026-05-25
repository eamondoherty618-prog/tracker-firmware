import { FeatureModule } from "@/types";

import { Badge } from "./Badge";
import { SectionCard } from "./SectionCard";

export function ComingSoonCard({ module }: { module: FeatureModule }) {
  return (
    <SectionCard className="p-5">
      <div className="flex items-start justify-between gap-3">
        <div>
          <h3 className="text-base font-semibold text-brand-ink">{module.name}</h3>
          <p className="mt-2 text-sm leading-6 text-slate-500">{module.description}</p>
        </div>
        <Badge tone={module.category === "comingSoon" ? "warning" : "active"}>
          {module.statusLabel}
        </Badge>
      </div>
    </SectionCard>
  );
}
