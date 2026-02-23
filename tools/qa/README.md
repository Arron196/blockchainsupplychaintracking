# QA Benchmark Milestone 1 (Ingest Success Rate + Latency)

This benchmark provides a reproducible baseline for `backend-cpp` ingest behavior in mock blockchain mode.

## What It Measures

- Ingest success rate (`accepted / total`) for deterministic packets.
- Client-observed request latency (`min`, `avg`, `p50`, `p95`, `max` in milliseconds).
- Server-side aggregate metrics from `/api/v1/metrics/overview`.

## Deterministic Setup

- Fixed payload generation (no randomness).
- Fixed base timestamp (`1700002000`) with sequential increments.
- Fixed `pubKeyId` (`pubkey-1`) and batch code (`BENCHMARK-QA-M1`).
- Ephemeral benchmark key material generated per run under `tools/qa/artifacts/keys/`.

## Run

From repository root:

```bash
tools/qa/run_ingest_benchmark.sh --requests 120
```

Optional output path:

```bash
tools/qa/run_ingest_benchmark.sh --output tools/qa/artifacts/m1_run.json --requests 120
```

## Output Artifact

Default artifact path:

- `tools/qa/artifacts/ingest_run_<timestamp>.json`

The JSON artifact includes:

- workload parameters
- success-rate summary
- latency distribution summary
- per-request records
- server metrics snapshot

## Notes

- Requires `openssl` CLI, `cmake`, `curl`, and `python3`.
- Runner starts backend in `AGRI_CHAIN_MODE=mock` and exits cleanly after benchmark completion.
