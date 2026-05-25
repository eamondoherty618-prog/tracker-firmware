import type { Config } from "@netlify/functions";

function json(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json",
      "cache-control": "no-store",
    },
  });
}

export default async () => {
  const { getStore } = await import("@netlify/blobs");
  const store = getStore({ name: "fleet-telemetry", consistency: "strong" });
  const { blobs } = await store.list({ prefix: "latest/" });
  const devices: Record<string, unknown> = {};

  for (const blob of blobs) {
    const record = await store.get(blob.key, { type: "json" });
    if (!record || typeof record !== "object") {
      continue;
    }
    const deviceId = String((record as { device_id?: string }).device_id ?? blob.key.replace(/^latest\//, ""));
    devices[deviceId] = record;
  }

  return json({ devices, ok: true });
};

export const config: Config = {
  path: "/api/fleet/latest",
  method: ["GET"],
};
