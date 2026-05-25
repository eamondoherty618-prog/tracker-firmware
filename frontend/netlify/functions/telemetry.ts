import type { Config, Context } from "@netlify/functions";

type TelemetryPayload = Record<string, unknown> & {
  device_id?: string;
};

function json(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json",
      "cache-control": "no-store",
    },
  });
}

async function saveTelemetry(payload: TelemetryPayload, context: Context) {
  const { getStore } = await import("@netlify/blobs");
  const store = getStore({ name: "fleet-telemetry", consistency: "strong" });
  const deviceId = String(payload.device_id ?? "").trim();
  if (!deviceId) {
    return json({ ok: false, error: "device_id is required" }, 400);
  }

  const receivedAt = new Date().toISOString();
  const record = {
    ...payload,
    device_id: deviceId,
    received_at: receivedAt,
    remote_addr: context.ip ?? null,
  };

  await store.setJSON(`latest/${deviceId}`, record);
  await store.setJSON(`events/${receivedAt}-${deviceId}`, record);

  return json({ ok: true, device_id: deviceId, received_at: receivedAt });
}

export default async (req: Request, context: Context) => {
  if (req.method === "POST") {
    try {
      const payload = (await req.json()) as TelemetryPayload;
      return await saveTelemetry(payload, context);
    } catch {
      return json({ ok: false, error: "invalid json body" }, 400);
    }
  }

  if (req.method === "GET") {
    const { getStore } = await import("@netlify/blobs");
    const store = getStore({ name: "fleet-telemetry", consistency: "strong" });
    const key = new URL(req.url).searchParams.get("device_id");
    if (!key) {
      return json({ ok: true, hint: "POST telemetry or query with ?device_id=tracker-001" });
    }

    const record = await store.get(`latest/${key}`, { type: "json" });
    if (!record) {
      return json({ ok: false, error: "not found" }, 404);
    }
    return json({ ok: true, device: record });
  }

  return new Response("Method not allowed", { status: 405 });
};

export const config: Config = {
  path: "/api/fleet/telemetry",
  method: ["GET", "POST"],
};
