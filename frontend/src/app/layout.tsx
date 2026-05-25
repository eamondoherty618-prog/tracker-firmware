import type { Metadata, Viewport } from "next";
import type { ReactNode } from "react";

import { AppShell } from "@/components/layout/AppShell";
import { PwaRegistrar } from "@/components/mobile/PwaRegistrar";
import { WorkspaceProvider } from "@/lib/workspace";

import "./globals.css";

export const metadata: Metadata = {
  title: "123 Mobile Track",
  description: "Fleet tracking SaaS dashboard for service vehicles and mobile fleets.",
  manifest: "/site.webmanifest",
  appleWebApp: {
    capable: true,
    statusBarStyle: "default",
    title: "123 Mobile Track",
  },
  icons: {
    icon: "/123-mobile-track-logo.png",
    apple: "/123-mobile-track-logo.png",
  },
};

export const viewport: Viewport = {
  themeColor: "#15803d",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: ReactNode;
}>) {
  return (
    <html lang="en">
      <body>
        <WorkspaceProvider>
          <PwaRegistrar />
          <AppShell>{children}</AppShell>
        </WorkspaceProvider>
      </body>
    </html>
  );
}
