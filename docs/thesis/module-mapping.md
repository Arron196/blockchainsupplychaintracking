# Thesis-to-Implementation Mapping (Milestone 1)

This mapping aligns current repository modules with the proposal document
`任相荣202204704开题报告最终.pdf` so thesis narrative and branch implementation stay synchronized.

## Section mapping

| Thesis section | Requirement from proposal | Implementation evidence | Status |
| --- | --- | --- | --- |
| 2.1(1) Define architecture, hardware/software modules | Split system into STM32, gateway, blockchain, and query surface | `README.md`, `docs/architecture/overview.md`, `backend-cpp/src/main.cpp` | Implemented at architecture level |
| 2.1(2) Build STM32 + sensor + comms hardware | Device-side sensing and transmission chain | `stm32-firmware/` skeleton exists, but no milestone evidence docs yet | In progress |
| 2.1(3) Implement data acquisition/processing/transmission and blockchain connection | Ingest parser, hash/signature checks, receipt flow | `backend-cpp/src/transport/json_parser.cpp`, `backend-cpp/src/ingest/ingest_service.cpp`, `backend-cpp/src/blockchain/mock_blockchain_client.cpp`, `backend-cpp/src/blockchain/ethereum_rpc_blockchain_client.cpp` | Implemented in backend |
| 2.1(4) End-to-end testing and optimization | Repeatable tests and measurable backend behavior | `backend-cpp/tests/test_ingest_service.cpp`, `backend-cpp/tests/test_json_parser.cpp`, `backend-cpp/tests/test_sqlite_repository.cpp`, CTest targets in `backend-cpp/CMakeLists.txt` | Implemented for backend scope |
| 2.2 Data integrity and authenticity | Validate packet hash and signature before storage | `backend-cpp/src/ingest/ingest_service.cpp`, `backend-cpp/src/security/basic_signature_verifier.cpp` | Implemented |
| 2.2 Tamper-evident traceability | Store telemetry + blockchain receipt and support trace queries | `backend-cpp/src/storage/sqlite_telemetry_repository.cpp`, routes in `backend-cpp/src/api/http_server.cpp` | Implemented |
| 2.2 Transmission success and latency metrics | Expose accepted/rejected counters and processing latency | `backend-cpp/include/domain/metrics_snapshot.h`, `GET /api/v1/metrics/overview` in `backend-cpp/src/api/http_server.cpp` | Implemented |
| 2.2 Power consumption evaluation | Provide power evidence for STM32 deployment | No power metric endpoint in backend; firmware-side measurement still required | Not implemented |

## Module ownership map

| Repository module | Thesis narrative role | Primary proof points |
| --- | --- | --- |
| `stm32-firmware/` | Sensor acquisition and signed packet producer | Proposal section 3.1/3.2 target; implementation evidence pending |
| `backend-cpp/src/transport` + `backend-cpp/src/security` + `backend-cpp/src/ingest` | Secure gateway path: parse -> verify -> accept/reject -> receipt | Parser defaults, hash check, signature verification, ingest result payload |
| `backend-cpp/src/storage` | Persistent evidence base for traceability and metrics denominator | SQLite schema, query paths by device, batch, and tx hash |
| `backend-cpp/src/blockchain` | Chain receipt binding and mode switch (mock/ethereum) | `AGRI_CHAIN_MODE`, receipt fields in ingest response |
| `backend-cpp/src/api/http_server.cpp` | Thesis experiment API surface and live demo interface | `/health`, `/api/v1/*`, `/ws/telemetry`, `/ws/alerts` |
| `docs/demo/runbook.md` | Demo execution narrative for thesis milestone review | Reproducible commands and expected outputs |
| `docs/thesis/metrics-evidence-template.md` | Experiment evidence capture template for chapter writing | Structured metric table + artifact checklist |

## Synchronization rules for PR-driven parallel work

Use this checklist whenever a branch changes backend routes, payload fields, or metrics math:

1. Update `docs/api/openapi-outline.md` in the same PR as route changes.
2. Re-run the commands in `docs/demo/runbook.md` and attach output artifacts.
3. Update `docs/thesis/metrics-evidence-template.md` with any new/removed metric fields.
4. If a thesis section claim changes, update this mapping table and cite exact module paths.
