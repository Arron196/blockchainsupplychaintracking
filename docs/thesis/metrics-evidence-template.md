# Metrics Evidence Template (Thesis Milestone)

Use this template to capture experiment evidence aligned with proposal section 2.2
(success ratio, latency, feasibility evidence).

## Experiment metadata

- Date:
- Branch:
- Commit hash:
- Operator:
- Environment: (OS, compiler, CMake version)
- Backend mode: (`AGRI_CHAIN_MODE=mock` or `AGRI_CHAIN_MODE=ethereum`)
- SQLite path:
- Public key directory:

## Commands executed

Record exact commands used (copy/paste from `docs/demo/runbook.md`).

```bash
cmake -S backend-cpp -B backend-cpp/build
cmake --build backend-cpp/build
ctest --test-dir backend-cpp/build --output-on-failure
```

```bash
curl -sS http://127.0.0.1:8080/health
curl -sS http://127.0.0.1:8080/api/v1/metrics/overview
```

## Raw evidence artifacts

- Backend startup log file:
- Ingest request payload file:
- Ingest response file(s):
- Metrics snapshot file(s):
- Device latest response file:
- Batch trace response file:
- Transaction response file:

## Metrics table (backend-supported)

Source endpoint: `GET /api/v1/metrics/overview`

| Metric name | Value | Source field | Notes |
| --- | --- | --- | --- |
| Total ingest requests |  | `totalRequests` | |
| Accepted ingest requests |  | `acceptedRequests` | |
| Rejected ingest requests |  | `rejectedRequests` | |
| Average processing latency (ms) |  | `averageProcessingMs` | Mean over all requests |
| Stored record count |  | `repositorySize` | SQLite-backed repository size |

Derived metric:

- Ingest success ratio (%) = `acceptedRequests / totalRequests * 100`

## Per-request latency sample (optional)

Source field: `processingMs` from each `POST /api/v1/ingest` response.

| Request index | HTTP status | accepted | processingMs | message |
| --- | --- | --- | --- | --- |
| 1 |  |  |  |  |
| 2 |  |  |  |  |
| 3 |  |  |  |  |

## Requirement coverage snapshot (proposal section 2.2)

| Proposal target | Current evidence | Result |
| --- | --- | --- |
| Data integrity/authenticity | hash check + signature verification results | |
| Tamper-evident traceability | tx hash query + batch trace response | |
| Transmission success ratio | computed from metrics endpoint | |
| Latency | `averageProcessingMs` and/or `processingMs` samples | |
| Power consumption | firmware-side measurement (not in backend API) | N/A or external evidence |

## Reviewer checklist

- [ ] Commands are reproducible on the recorded commit.
- [ ] Endpoint responses are attached as raw files, not paraphrased.
- [ ] Derived values match raw JSON fields.
- [ ] Any N/A item includes a concrete reason and owner module.
