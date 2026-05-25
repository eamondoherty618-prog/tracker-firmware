"use client";

import { ReactNode, useState } from "react";

import { StickyFilterPanel, MobileFilterDrawer } from "@/components/filters/StickyFilterPanel";
import { SidebarNav } from "@/components/layout/SidebarNav";
import { TopHeader } from "@/components/layout/TopHeader";
import { PwaInstallPrompt } from "@/components/mobile/PwaInstallPrompt";

export function AppShell({ children }: { children: ReactNode }) {
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false);
  const [filtersOpen, setFiltersOpen] = useState(false);

  return (
    <div className="min-h-screen bg-brand-cloud">
      <div className="mx-auto flex min-h-screen max-w-[1720px]">
        <SidebarNav
          collapsed={sidebarCollapsed}
          onToggle={() => setSidebarCollapsed((value) => !value)}
        />

        <div className="flex min-w-0 flex-1 flex-col">
          <TopHeader onOpenFilters={() => setFiltersOpen(true)} />
          <main className="flex-1 px-4 py-5 lg:px-6">
            <div className="mb-5 xl:hidden">
              <PwaInstallPrompt />
            </div>
            <div className="grid gap-5 xl:grid-cols-[280px_minmax(0,1fr)]">
              <StickyFilterPanel />
              <div className="min-w-0">{children}</div>
            </div>
          </main>
        </div>
      </div>

      <MobileFilterDrawer open={filtersOpen} onClose={() => setFiltersOpen(false)} />
    </div>
  );
}
