"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { PanelLeftClose, PanelLeftOpen } from "lucide-react";

import { BrandLogo } from "@/components/branding/BrandLogo";
import { navigationItems } from "@/lib/navigation";
import { useWorkspace } from "@/lib/workspace";
import { cn } from "@/lib/utils";

interface SidebarNavProps {
  collapsed: boolean;
  onToggle: () => void;
}

export function SidebarNav({ collapsed, onToggle }: SidebarNavProps) {
  const pathname = usePathname();
  const { state, serviceArea, hasServiceArea } = useWorkspace();

  return (
    <aside
      className={cn(
        "hidden border-r border-brand-line bg-[#f9fbfd] px-4 py-4 lg:flex lg:flex-col",
        collapsed ? "lg:w-[96px]" : "lg:w-[272px]",
      )}
    >
      <div className="flex items-center justify-between gap-3">
        <BrandLogo collapsed={collapsed} />
        <button
          aria-label="Toggle sidebar"
          onClick={onToggle}
          className="rounded-md border border-brand-line bg-white p-2 text-brand-ink"
        >
          {collapsed ? <PanelLeftOpen size={18} /> : <PanelLeftClose size={18} />}
        </button>
      </div>

      <nav className="mt-6 space-y-1">
        {navigationItems.map((item) => {
          const active = pathname.startsWith(item.href);
          const Icon = item.icon;
          return (
            <Link
              key={item.href}
              href={item.href}
              className={cn(
                "flex h-11 items-center gap-3 rounded-md px-3 text-sm font-medium transition",
                active
                  ? "bg-brand-navy text-white shadow-soft"
                  : "text-brand-text hover:bg-white hover:text-brand-ink",
                collapsed && "justify-center px-0",
              )}
            >
              <Icon size={18} />
              {!collapsed && <span>{item.label}</span>}
            </Link>
          );
        })}
      </nav>

      <div className="mt-auto rounded-lg border border-brand-line bg-white p-4 shadow-soft">
        {!collapsed ? (
          <>
            <p className="text-xs font-semibold uppercase text-brand-forest">Service area</p>
            <h3 className="mt-2 text-sm font-semibold text-brand-ink">
              {hasServiceArea ? serviceArea.label : "Not set"}
            </h3>
            <p className="mt-2 text-xs leading-5 text-slate-500">
              {state.vehicles.length} vehicle{state.vehicles.length === 1 ? "" : "s"}, {state.drivers.length} driver
              {state.drivers.length === 1 ? "" : "s"}, tracker-001 online.
            </p>
          </>
        ) : (
          <div className="text-center text-xs font-semibold text-brand-forest">123</div>
        )}
      </div>
    </aside>
  );
}
