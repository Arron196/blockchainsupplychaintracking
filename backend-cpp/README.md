# backend-cpp

Edge gateway service responsibilities:

- Receive signed telemetry from STM32 devices.
- Verify ECDSA signatures and packet integrity.
- Persist raw telemetry and normalized metrics.
- Submit hash metadata to blockchain.
- Provide REST + WebSocket APIs for frontend dashboards.

## Runtime Modes

- Storage: SQLite only (`AGRI_SQLITE_PATH`, default `backend-cpp/data/agri_gateway.db`).
- Blockchain:
  - `AGRI_CHAIN_MODE=mock` for local development.
  - `AGRI_CHAIN_MODE=ethereum` for real JSON-RPC node.

## Ethereum RPC Environment

- `AGRI_ETH_RPC_URL` (default `http://127.0.0.1:8545`)
- `AGRI_ETH_FROM` (required for ethereum mode)
- `AGRI_ETH_TO` (optional, defaults to `AGRI_ETH_FROM`)
- `AGRI_ETH_POLL_MS` (default `500`)
- `AGRI_ETH_MAX_WAIT_MS` (default `15000`)

## WebSocket Channels

- `WS /ws/telemetry` for accepted ingest events.
- `WS /ws/alerts` for rejected ingest events.
