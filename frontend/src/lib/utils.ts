import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function statusTone(status: string) {
  switch (status) {
    case "moving":
    case "online":
    case "completed":
    case "active":
      return "bg-green-50 text-green-700 border-green-200";
    case "idle":
    case "warning":
    case "review":
    case "due-soon":
      return "bg-amber-50 text-amber-700 border-amber-200";
    case "offline":
    case "critical":
    case "overdue":
      return "bg-red-50 text-red-700 border-red-200";
    default:
      return "bg-slate-50 text-slate-700 border-slate-200";
  }
}
