"use client";

import { useMemo, useState } from "react";
import { Bell, CalendarRange, Filter, Search } from "lucide-react";

import { BrandLogo } from "@/components/branding/BrandLogo";
import { Button } from "@/components/ui/Button";
import { useWorkspace } from "@/lib/workspace";

interface TopHeaderProps {
  onOpenFilters: () => void;
}

export function TopHeader({ onOpenFilters }: TopHeaderProps) {
  const { state, notifications, setDateRange } = useWorkspace();
  const [showNotifications, setShowNotifications] = useState(false);
  const dateRanges = useMemo(() => ["Today", "Last 7 days", "Last 30 days", "This month"] as const, []);

  return (
    <header className="sticky top-0 z-20 border-b border-brand-line bg-white/90 backdrop-blur">
      <div className="flex flex-wrap items-center gap-3 px-4 py-4 lg:px-6">
        <BrandLogo className="lg:hidden" />

        <div className="relative min-w-[260px] flex-1">
          <Search size={16} className="pointer-events-none absolute left-3 top-1/2 -translate-y-1/2 text-slate-400" />
          <input
            className="h-11 w-full rounded-md border border-brand-line bg-brand-cloud pl-9 pr-4 text-sm outline-none transition focus:border-brand-forest"
            placeholder="Search vehicles, devices, drivers, plates, regions..."
          />
        </div>

        <div className="flex items-center gap-2">
          <button
            onClick={() => {
              const currentIndex = dateRanges.indexOf(state.dateRange);
              const nextValue = dateRanges[(currentIndex + 1) % dateRanges.length];
              setDateRange(nextValue);
            }}
            className="inline-flex h-11 items-center gap-2 rounded-md border border-brand-line px-3 text-sm font-medium text-brand-text"
          >
            <CalendarRange size={16} />
            {state.dateRange}
          </button>
          <div className="relative">
            <button
              onClick={() => setShowNotifications((current) => !current)}
              className="relative inline-flex h-11 w-11 items-center justify-center rounded-md border border-brand-line text-brand-text"
            >
              <Bell size={18} />
              {notifications.length > 0 && <span className="absolute right-2 top-2 h-2.5 w-2.5 rounded-full bg-red-500" />}
            </button>
            {showNotifications && (
              <div className="absolute right-0 top-14 z-30 w-80 rounded-lg border border-brand-line bg-white p-3 shadow-panel">
                <p className="text-sm font-semibold text-brand-ink">Account updates</p>
                <div className="mt-3 space-y-2">
                  {notifications.length > 0 ? (
                    notifications.map((item) => (
                      <div key={item.id} className="rounded-md border border-brand-line bg-brand-cloud px-3 py-3">
                        <p className="text-sm font-semibold text-brand-ink">{item.title}</p>
                        <p className="mt-1 text-xs leading-5 text-slate-500">{item.detail}</p>
                      </div>
                    ))
                  ) : (
                    <p className="text-sm text-slate-500">Everything looks good right now.</p>
                  )}
                </div>
              </div>
            )}
          </div>
          <Button variant="secondary" onClick={onOpenFilters} className="lg:hidden">
            <Filter size={16} className="mr-2" />
            Filters
          </Button>
          <div className="hidden h-11 items-center gap-3 rounded-md border border-brand-line px-3 lg:flex">
            <div className="h-8 w-8 rounded-full bg-brand-forest text-center text-sm font-bold leading-8 text-white">
              {state.adminName.split(" ").map((part) => part[0]).join("").slice(0, 2)}
            </div>
            <div className="text-left">
              <p className="text-sm font-semibold text-brand-ink">{state.adminName}</p>
              <p className="text-xs text-slate-500">{state.companyName}</p>
            </div>
          </div>
        </div>
      </div>
    </header>
  );
}
