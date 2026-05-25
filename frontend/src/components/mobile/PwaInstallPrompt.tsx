"use client";

import { useEffect, useMemo, useState } from "react";
import { Download, Share2, Smartphone, X } from "lucide-react";

import { Button } from "@/components/ui/Button";

type BeforeInstallPromptEvent = Event & {
  prompt: () => Promise<void>;
  userChoice: Promise<{ outcome: "accepted" | "dismissed"; platform: string }>;
};

function isIos() {
  if (typeof window === "undefined") return false;
  return /iphone|ipad|ipod/i.test(window.navigator.userAgent);
}

function isStandalone() {
  if (typeof window === "undefined") return false;
  return window.matchMedia("(display-mode: standalone)").matches || (window.navigator as Navigator & { standalone?: boolean }).standalone === true;
}

export function PwaInstallPrompt() {
  const [deferredPrompt, setDeferredPrompt] = useState<BeforeInstallPromptEvent | null>(null);
  const [dismissed, setDismissed] = useState(false);
  const [installed, setInstalled] = useState(false);

  useEffect(() => {
    if (isStandalone()) {
      setInstalled(true);
      return;
    }

    function onBeforeInstallPrompt(event: Event) {
      event.preventDefault();
      setDeferredPrompt(event as BeforeInstallPromptEvent);
    }

    function onInstalled() {
      setInstalled(true);
      setDeferredPrompt(null);
    }

    window.addEventListener("beforeinstallprompt", onBeforeInstallPrompt);
    window.addEventListener("appinstalled", onInstalled);

    return () => {
      window.removeEventListener("beforeinstallprompt", onBeforeInstallPrompt);
      window.removeEventListener("appinstalled", onInstalled);
    };
  }, []);

  const mode = useMemo(() => {
    if (installed || dismissed) return "hidden";
    if (deferredPrompt) return "install";
    if (isIos() && !isStandalone()) return "ios";
    return "hidden";
  }, [deferredPrompt, dismissed, installed]);

  if (mode === "hidden") return null;

  return (
    <div className="rounded-lg border border-brand-forest/20 bg-emerald-50 px-4 py-4 shadow-soft">
      <div className="flex items-start justify-between gap-4">
        <div className="flex min-w-0 gap-3">
          <div className="rounded-md bg-white p-2 text-brand-forest">
            <Smartphone size={18} />
          </div>
          <div className="min-w-0">
            <p className="text-sm font-semibold text-brand-ink">Install 123 Mobile Track</p>
            {mode === "install" ? (
              <p className="mt-1 text-sm text-slate-600">
                Add it to your home screen for a full-screen mobile app experience with faster launch and offline shell support.
              </p>
            ) : (
              <p className="mt-1 text-sm text-slate-600">
                On iPhone, tap <Share2 className="mx-1 inline-block" size={14} /> then choose <span className="font-semibold text-brand-ink">Add to Home Screen</span>.
              </p>
            )}
          </div>
        </div>
        <button
          onClick={() => setDismissed(true)}
          className="rounded-md border border-brand-line bg-white p-2 text-brand-ink"
          aria-label="Dismiss install prompt"
        >
          <X size={16} />
        </button>
      </div>

      {mode === "install" && (
        <div className="mt-4">
          <Button
            onClick={async () => {
              if (!deferredPrompt) return;
              await deferredPrompt.prompt();
              await deferredPrompt.userChoice;
              setDeferredPrompt(null);
            }}
          >
            <Download size={16} className="mr-2" />
            Install App
          </Button>
        </div>
      )}
    </div>
  );
}
