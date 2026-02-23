export interface BlockchainReceipt {
  txHash: string;
  blockHeight: number;
  submittedAt: string;
}

export interface TelemetryRecord {
  recordId: number;
  deviceId: string;
  timestamp: number;
  telemetry: unknown;
  hash: string;
  signature: string;
  pubKeyId: string;
  transport: string;
  batchCode?: string;
  receipt: BlockchainReceipt | null;
}

export interface MetricsOverview {
  totalRequests: number;
  acceptedRequests: number;
  rejectedRequests: number;
  averageProcessingMs: number;
  repositorySize: number;
}

export interface BatchTraceResponse {
  batchCode: string;
  count: number;
  records: TelemetryRecord[];
}

export interface TelemetryIngestedEvent {
  type: "telemetry.ingested";
  deviceId: string;
  recordId: number;
  timestamp: number;
  transport: string;
  txHash: string;
}

export interface IngestRejectedEvent {
  type: "ingest.rejected";
  deviceId: string;
  message: string;
}
