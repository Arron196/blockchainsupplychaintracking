import { describe, expect, it } from "vitest";

import { isAlertEvent, isTelemetryEvent, parseDashboardEvent } from "@/websocket/events";

describe("parseDashboardEvent", () => {
  it("parses telemetry events with expected shape", () => {
    const parsed = parseDashboardEvent({
      type: "telemetry.ingested",
      deviceId: "stm32-1",
      recordId: 12,
      timestamp: 1700000100,
      transport: "wifi",
      txHash: "0xabc"
    });

    expect(parsed).not.toBeNull();
    expect(parsed && isTelemetryEvent(parsed)).toBe(true);
  });

  it("parses alert events with expected shape", () => {
    const parsed = parseDashboardEvent({
      type: "ingest.rejected",
      deviceId: "stm32-1",
      message: "signature mismatch"
    });

    expect(parsed).not.toBeNull();
    expect(parsed && isAlertEvent(parsed)).toBe(true);
  });

  it("rejects unknown event types and malformed payloads", () => {
    expect(parseDashboardEvent({ type: "unknown" })).toBeNull();
    expect(parseDashboardEvent({ type: "telemetry.ingested", recordId: "x" })).toBeNull();
    expect(parseDashboardEvent("not-an-object")).toBeNull();
  });
});
