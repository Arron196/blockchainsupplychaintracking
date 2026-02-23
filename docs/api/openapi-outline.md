# API Outline (Implementation Baseline)

This document reflects the current behavior implemented in
`backend-cpp/src/api/http_server.cpp` and `backend-cpp/src/transport/json_parser.cpp`.

## Health

- `GET /health`
  - `200` response body: `{"status":"ok"}`

## Ingest

- `POST /api/v1/ingest`
  - Required request fields:
    - `deviceId` (string)
    - `timestamp` (unsigned integer)
    - `telemetry` (JSON object)
    - `hash` (64-char hex string)
    - `signature` (string)
  - Optional request fields:
    - `pubKeyId` (default: `default-pubkey`)
    - `transport` (default: `wifi`)
    - `batchCode` (default: empty)
  - `202` response body fields on accepted ingest:
    - `accepted`, `message`, `recordId`, `processingMs`, `receipt`
  - `400` response body fields on rejected ingest:
    - `accepted`, `message`, `recordId`, `processingMs`, `receipt`
    - parser errors may return `{"error":"..."}`

## Query

- `GET /api/v1/metrics/overview`
  - `200` response fields: `totalRequests`, `acceptedRequests`, `rejectedRequests`,
    `averageProcessingMs`, `repositorySize`
- `GET /api/v1/devices/{deviceId}/latest`
  - `200` response: telemetry record with packet and optional `receipt`
  - `404` response body: `{"error":"device not found"}`
- `GET /api/v1/batches/{batchCode}/trace`
  - `200` response fields: `batchCode`, `count`, `records[]`
- `GET /api/v1/transactions/{txHash}`
  - `200` response: telemetry record with matching transaction hash
  - `404` response body: `{"error":"transaction not found"}`

## Realtime

- `WS /ws/telemetry`
  - Event type: `telemetry.ingested`
  - Event fields: `type`, `deviceId`, `recordId`, `timestamp`, `transport`, `txHash`
- `WS /ws/alerts`
  - Event type: `ingest.rejected`
  - Event fields: `type`, `deviceId`, `message`
