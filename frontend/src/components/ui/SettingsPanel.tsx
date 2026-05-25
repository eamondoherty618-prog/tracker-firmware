import { SectionCard } from "./SectionCard";

const settingsGroups = [
  {
    title: "Organization",
    items: ["Company profile", "Branding assets", "Timezone and locale", "Support contacts"],
    status: "Available in Settings",
  },
  {
    title: "Notifications",
    items: ["Alert routing", "Email summaries", "SMS escalation", "Quiet hours"],
    status: "Next up",
  },
  {
    title: "Device Defaults",
    items: ["Reporting defaults", "Motion wake policy", "Deep sleep defaults", "Firmware rollout channels"],
    status: "Next up",
  },
  {
    title: "Platform",
    items: ["Billing", "API and integrations", "User roles", "Audit log exports"],
    status: "Later",
  },
];

export function SettingsPanel() {
  return (
    <div className="grid gap-5 xl:grid-cols-2">
      {settingsGroups.map((group) => (
        <SectionCard key={group.title} className="p-5">
          <h3 className="text-base font-semibold text-brand-ink">{group.title}</h3>
          <div className="mt-4 space-y-3">
            {group.items.map((item) => (
              <div
                key={item}
                className="flex items-center justify-between rounded-md border border-brand-line px-4 py-3"
              >
                <span className="text-sm font-medium text-brand-text">{item}</span>
                <span className="text-xs font-semibold text-slate-400">{group.status}</span>
              </div>
            ))}
          </div>
        </SectionCard>
      ))}
    </div>
  );
}
