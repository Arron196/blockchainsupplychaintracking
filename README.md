# Agricultural Traceability System (STM32 + Blockchain)

This repository is organized for a graduation project with a clear split between firmware, gateway backend, and frontend demo.

## Repository Layout

```text
.
├── backend-cpp/               # C++ edge gateway service
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── api/
│   │   ├── blockchain/
│   │   ├── config/
│   │   ├── domain/
│   │   ├── dto/
│   │   ├── metrics/
│   │   ├── security/
│   │   ├── services/
│   │   ├── storage/
│   │   └── transport/
│   ├── src/
│   │   ├── api/{controllers,routes}/
│   │   ├── blockchain/
│   │   ├── ingest/
│   │   ├── security/
│   │   ├── simulation/
│   │   ├── storage/
│   │   ├── ws/
│   │   └── main.cpp
│   ├── tests/
│   └── scripts/
├── frontend/                  # Demo frontend (dashboard + traceability)
│   ├── public/
│   ├── src/
│   │   ├── api/
│   │   ├── assets/
│   │   ├── components/
│   │   ├── layouts/
│   │   ├── mocks/
│   │   ├── pages/
│   │   │   ├── dashboard/
│   │   │   ├── devices/
│   │   │   ├── operations/
│   │   │   └── traceability/
│   │   ├── router/
│   │   ├── stores/
│   │   ├── styles/
│   │   ├── types/
│   │   └── websocket/
├── stm32-firmware/            # Device-side firmware project
│   ├── App/
│   │   ├── include/
│   │   └── src/
│   ├── BSP/sensors/
│   ├── Config/cubemx/
│   ├── Core/{Inc,Src}/
│   ├── Drivers/
│   ├── Middleware/crypto/
│   ├── scripts/
│   └── tests/
├── docs/
│   ├── api/
│   ├── architecture/
│   ├── demo/
│   └── thesis/
├── infra/
│   ├── compose/
│   ├── docker/
│   └── observability/
├── shared/
│   ├── contracts/
│   ├── schemas/
│   └── testdata/
└── tools/
    ├── seed/
    └── simulator/
```

## Suggested Development Order

1. Implement `stm32-firmware/` data acquisition, signing, and packet format.
2. Implement `backend-cpp/` ingest API, signature verification, and persistence.
3. Add blockchain adapter and transaction receipt flow in `backend-cpp/`.
4. Implement frontend dashboards in `frontend/`.
5. Add end-to-end demo scripts in `tools/simulator/` and `docs/demo/`.
