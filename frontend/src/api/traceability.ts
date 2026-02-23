import { fetchJson } from "@/api/http";
import type { BatchTraceResponse, TelemetryRecord } from "@/types/contracts";

export const fetchBatchTrace = async (batchCode: string): Promise<BatchTraceResponse> =>
  fetchJson<BatchTraceResponse>(`/api/v1/batches/${encodeURIComponent(batchCode)}/trace`);

export const fetchDeviceLatest = async (deviceId: string): Promise<TelemetryRecord> =>
  fetchJson<TelemetryRecord>(`/api/v1/devices/${encodeURIComponent(deviceId)}/latest`);

export const fetchTransaction = async (txHash: string): Promise<TelemetryRecord> =>
  fetchJson<TelemetryRecord>(`/api/v1/transactions/${encodeURIComponent(txHash)}`);
