"use client";

import Image from "next/image";
import Link from "next/link";

import { useWorkspace } from "@/lib/workspace";
import { cn } from "@/lib/utils";

interface BrandLogoProps {
  collapsed?: boolean;
  className?: string;
}

export function BrandLogo({ collapsed = false, className }: BrandLogoProps) {
  const { state } = useWorkspace();

  return (
    <Link
      href="/dashboard"
      className={cn(
        "flex items-center gap-3 rounded-lg border border-brand-line bg-white px-3 py-3 shadow-soft",
        collapsed && "justify-center px-2",
        className,
      )}
    >
      <div className="relative h-12 w-12 shrink-0 overflow-hidden rounded-full border border-brand-line bg-brand-mint">
        <Image
          src="/123-mobile-track-logo.png"
          alt="123 Mobile Track logo"
          fill
          className="object-cover"
          priority
        />
      </div>
      {!collapsed && (
        <div className="min-w-0">
          <p className="text-xs font-semibold uppercase tracking-normal text-brand-forest">Fleet tracking</p>
          <p className="truncate text-sm font-bold text-brand-ink">{state.companyName}</p>
        </div>
      )}
    </Link>
  );
}
