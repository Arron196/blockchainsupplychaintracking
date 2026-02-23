# API Outline (Draft)

## Ingest

- `POST /api/v1/ingest`
  - Request: deviceId, timestamp, telemetry, hash, signature, pubKeyId
  - Response: accepted, message, recordId, processingMs, receipt

## Query

- `GET /api/v1/devices/{deviceId}/latest`
- `GET /api/v1/batches/{batchCode}/trace`
- `GET /api/v1/transactions/{txHash}`
- `GET /api/v1/metrics/overview`

## Realtime

- `WS /ws/telemetry`
  - Event: `telemetry.ingested`
- `WS /ws/alerts`
  - Event: `ingest.rejected`
