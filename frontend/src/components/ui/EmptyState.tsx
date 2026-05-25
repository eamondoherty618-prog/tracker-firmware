import { ReactNode } from "react";

import { Button } from "./Button";
import { SectionCard } from "./SectionCard";

export function EmptyState({
  title,
  description,
  actionLabel,
  onAction,
  icon,
}: {
  title: string;
  description: string;
  actionLabel?: string;
  onAction?: () => void;
  icon?: ReactNode;
}) {
  return (
    <SectionCard className="flex min-h-64 flex-col items-center justify-center p-10 text-center">
      <div className="mb-4 rounded-full bg-brand-mint p-4 text-brand-forest">{icon}</div>
      <h3 className="text-lg font-semibold text-brand-ink">{title}</h3>
      <p className="mt-2 max-w-md text-sm text-slate-500">{description}</p>
      {actionLabel && (
        <Button className="mt-5" onClick={onAction}>
          {actionLabel}
        </Button>
      )}
    </SectionCard>
  );
}
