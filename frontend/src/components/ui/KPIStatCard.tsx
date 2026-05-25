import { KPIStat } from "@/types";

import { SectionCard } from "./SectionCard";

const toneClasses = {
  green: "bg-green-50 text-green-700",
  navy: "bg-brand-cloud text-brand-navy",
  amber: "bg-amber-50 text-amber-700",
  red: "bg-red-50 text-red-700",
};

export function KPIStatCard({ stat }: { stat: KPIStat }) {
  return (
    <SectionCard className="p-5">
      <div className="flex items-start justify-between gap-4">
        <div>
          <p className="text-sm font-medium text-slate-500">{stat.label}</p>
          <p className="mt-3 text-3xl font-bold text-brand-ink">{stat.value}</p>
        </div>
        <span
          className={`rounded-md px-2 py-1 text-xs font-semibold ${toneClasses[stat.tone]}`}
        >
          {stat.delta}
        </span>
      </div>
    </SectionCard>
  );
}
