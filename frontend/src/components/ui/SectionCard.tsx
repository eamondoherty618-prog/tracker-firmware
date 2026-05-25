import { ReactNode } from "react";

import { cn } from "@/lib/utils";

export function SectionCard({
  children,
  className,
}: {
  children: ReactNode;
  className?: string;
}) {
  return (
    <section
      className={cn(
        "rounded-lg border border-brand-line bg-white shadow-soft",
        className,
      )}
    >
      {children}
    </section>
  );
}
