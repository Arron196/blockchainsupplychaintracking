import type { IngestRejectedEvent, TelemetryIngestedEvent } from "@/types/contracts";

export type DashboardEvent = TelemetryIngestedEvent | IngestRejectedEvent;

const isObjectRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null;

const isString = (value: unknown): value is string => typeof value === "string";

const isFiniteNumber = (value: unknown): value is number =>
  typeof value === "number" && Number.isFinite(value);

export const isTelemetryEvent = (event: DashboardEvent): event is TelemetryIngestedEvent =>
  event.type === "telemetry.ingested";

export const isAlertEvent = (event: DashboardEvent): event is IngestRejectedEvent =>
  event.type === "ingest.rejected";

export const parseDashboardEvent = (value: unknown): DashboardEvent | null => {
  if (!isObjectRecord(value) || !isString(value.type)) {
    return null;
  }

  if (value.type === "telemetry.ingested") {
    if (
      isString(value.deviceId) &&
      isFiniteNumber(value.recordId) &&
      isFiniteNumber(value.timestamp) &&
      isString(value.transport) &&
      isString(value.txHash)
    ) {
      return {
        type: "telemetry.ingested",
        deviceId: value.deviceId,
        recordId: value.recordId,
        timestamp: value.timestamp,
        transport: value.transport,
        txHash: value.txHash
      };
    }
    return null;
  }

  if (value.type === "ingest.rejected") {
    if (isString(value.deviceId) && isString(value.message)) {
      return {
        type: "ingest.rejected",
        deviceId: value.deviceId,
        message: value.message
      };
    }
    return null;
  }

  return null;
};
