import type { IngestRejectedEvent, TelemetryIngestedEvent } from "@/types/contracts";

export type DashboardEvent = TelemetryIngestedEvent | IngestRejectedEvent;

export const isTelemetryEvent = (event: DashboardEvent): event is TelemetryIngestedEvent =>
  event.type === "telemetry.ingested";

export const isAlertEvent = (event: DashboardEvent): event is IngestRejectedEvent =>
  event.type === "ingest.rejected";
