# frontend

Frontend contains two demo perspectives:

- Operations dashboard: system health, alerts, throughput, latency.
- Consumer traceability: batch lookup, timeline, and proof display.

Recommended stack: Vue 3 + TypeScript + Vite + Pinia + ECharts.

## Milestone 1 scaffold

- Routes:
  - `/operations`
  - `/traceability`
- Contract-aligned API clients:
  - `GET /api/v1/metrics/overview`
  - `GET /api/v1/devices/{deviceId}/latest`
  - `GET /api/v1/batches/{batchCode}/trace`
  - `GET /api/v1/transactions/{txHash}`
- WebSocket clients:
  - `WS /ws/telemetry` (`telemetry.ingested`)
  - `WS /ws/alerts` (`ingest.rejected`)

## Local run

```bash
npm install
npm run dev
```

## Environment variables

Create `.env.local` in `frontend/` when backend host differs from defaults:

```bash
VITE_API_BASE_URL=http://127.0.0.1:8080
VITE_WS_BASE_URL=ws://127.0.0.1:8080
```

- `VITE_API_BASE_URL`: REST API base URL.
- `VITE_WS_BASE_URL`: WebSocket base URL used by telemetry/alerts channels.
