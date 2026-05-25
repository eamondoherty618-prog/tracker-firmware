import { ComingSoonCard } from "@/components/ui/ComingSoonCard";
import { SectionCard } from "@/components/ui/SectionCard";
import { featureModules } from "@/data/mockData";

export default function FeaturesPage() {
  const active = featureModules.filter((item) => item.category === "active");
  const beta = featureModules.filter((item) => item.category === "beta");
  const comingSoon = featureModules.filter((item) => item.category === "comingSoon");
  const hardware = featureModules.filter((item) => item.category === "hardware");

  return (
    <div className="space-y-6">
      <div>
        <p className="text-sm font-semibold uppercase text-brand-forest">Features</p>
        <h1 className="mt-1 text-3xl font-bold text-brand-ink">Features</h1>
        <p className="mt-2 text-sm text-slate-500">
          See what is available now, what depends on hardware, and what is planned next.
        </p>
      </div>

      {([
        ["Ready now", active],
        ["In beta", beta],
        ["Planned", comingSoon],
        ["Hardware add-ons", hardware],
      ] as const).map(([title, items]) => (
        <section key={title as string} className="space-y-4">
          <div className="flex items-center justify-between">
            <h2 className="text-lg font-semibold text-brand-ink">{title}</h2>
            <span className="text-sm text-slate-500">{items.length} modules</span>
          </div>
          <div className="grid gap-4 xl:grid-cols-2">
            {items.map((module) => (
              <ComingSoonCard key={module.id} module={module} />
            ))}
          </div>
        </section>
      ))}

      <SectionCard className="p-5">
        <h2 className="text-base font-semibold text-brand-ink">Availability</h2>
        <p className="mt-2 text-sm leading-6 text-slate-500">
          Active features are ready to use in the account today. Beta features may change as the platform grows.
          Hardware-based features need compatible tracker wiring or accessories before they can be turned on.
        </p>
      </SectionCard>
    </div>
  );
}
