import { ReactNode } from "react";

import { cn, statusTone } from "@/lib/utils";

export function Badge({
  children,
  tone,
}: {
  children: ReactNode;
  tone?: string;
}) {
  return (
    <span
      className={cn(
        "inline-flex items-center rounded-full border px-2.5 py-1 text-xs font-semibold",
        tone ? statusTone(tone) : "border-brand-line bg-white text-brand-text",
      )}
    >
      {children}
    </span>
  );
}
