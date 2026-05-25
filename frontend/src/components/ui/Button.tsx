import { ButtonHTMLAttributes, forwardRef } from "react";

import { cn } from "@/lib/utils";

type Variant = "primary" | "secondary" | "ghost" | "danger";

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: Variant;
}

export const Button = forwardRef<HTMLButtonElement, ButtonProps>(function Button(
  { className, variant = "primary", ...props },
  ref,
) {
  return (
    <button
      ref={ref}
      className={cn(
        "inline-flex h-10 items-center justify-center rounded-md px-4 text-sm font-semibold transition",
        "focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-brand-forest/40 disabled:cursor-not-allowed disabled:opacity-60",
        variant === "primary" && "bg-brand-forest text-white hover:bg-green-700",
        variant === "secondary" &&
          "border border-brand-line bg-white text-brand-ink hover:bg-brand-cloud",
        variant === "ghost" && "text-brand-ink hover:bg-brand-cloud",
        variant === "danger" && "bg-red-600 text-white hover:bg-red-700",
        className,
      )}
      {...props}
    />
  );
});
