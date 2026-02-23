# System Architecture Overview

## Data path

1. STM32 reads sensors and forms a telemetry packet.
2. Firmware signs packet and sends to edge gateway.
3. C++ backend verifies signature and stores telemetry.
4. Backend submits hash proof to blockchain and stores tx receipt.
5. Frontend consumes REST/WS APIs for real-time and traceability views.

## Core quality goals

- End-to-end integrity (tamper evidence)
- High ingest success ratio
- Predictable latency for dashboard and trace lookup
- Recoverability under network disruptions
