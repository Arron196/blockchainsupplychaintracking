# Demo Runbook (Backend Evidence Path)

This runbook is for the current `backend-cpp` implementation and is intended to produce
repeatable evidence for thesis demos.

## 1) Build and unit-test backend

```bash
cmake -S backend-cpp -B backend-cpp/build
cmake --build backend-cpp/build
ctest --test-dir backend-cpp/build --output-on-failure
```

## 2) Start gateway (Terminal A)

```bash
AGRI_CHAIN_MODE=mock \
AGRI_SQLITE_PATH=backend-cpp/data/agri_gateway.db \
AGRI_PUBLIC_KEYS_DIR=backend-cpp/keys/public \
./backend-cpp/build/agri_gateway
```

Expected startup lines include:

- `agri_gateway listening on 0.0.0.0:8080`
- `routes: /health, /api/v1/ingest, /api/v1/metrics/overview, /ws/telemetry, /ws/alerts`

## 3) Health check (Terminal B)

```bash
curl -sS http://127.0.0.1:8080/health
```

Expected response:

```json
{"status":"ok"}
```

## 4) Create and submit one telemetry packet

The command block below first tries the non-OpenSSL demo signature format
`<hash>:<pubKeyId>`. If the backend rejects with `signature verification failed`, it retries
with an ECDSA signature using the test private key paired with
`backend-cpp/keys/public/pubkey-1.pem` (same keypair embedded in `backend-cpp/tests/test_ingest_service.cpp`).

```bash
set -euo pipefail

export BASE_URL="http://127.0.0.1:8080"
export DEVICE_ID="stm32-node-1"
export BATCH_CODE="BATCH-2026-0001"
export TELEMETRY_JSON='{"temperature":24.5,"humidity":62.3}'
export TS="$(python3 - <<'PY'
import time
print(int(time.time()))
PY
)"

export HASH="$(python3 - <<'PY'
import hashlib
import os
canonical = f"{os.environ['DEVICE_ID']}|{os.environ['TS']}|{os.environ['TELEMETRY_JSON']}"
print(hashlib.sha256(canonical.encode()).hexdigest())
PY
)"

write_payload() {
  python3 - <<'PY'
import json
import os
payload = {
    "deviceId": os.environ["DEVICE_ID"],
    "timestamp": int(os.environ["TS"]),
    "telemetry": json.loads(os.environ["TELEMETRY_JSON"]),
    "hash": os.environ["HASH"],
    "signature": os.environ["SIGNATURE"],
    "pubKeyId": "pubkey-1",
    "transport": "wifi",
    "batchCode": os.environ["BATCH_CODE"],
}
with open("/tmp/agri_ingest.json", "w", encoding="utf-8") as f:
    json.dump(payload, f, separators=(",", ":"))
PY
}

send_ingest() {
  write_payload
  curl -sS -o /tmp/agri_ingest_response.json -w '%{http_code}' \
    -H 'Content-Type: application/json' \
    --data-binary @/tmp/agri_ingest.json \
    "$BASE_URL/api/v1/ingest"
}

export SIGNATURE="$HASH:pubkey-1"
HTTP_CODE="$(send_ingest)"

if [ "$HTTP_CODE" = "400" ]; then
  ERROR_MSG="$(python3 - <<'PY'
import json
try:
    with open('/tmp/agri_ingest_response.json', 'r', encoding='utf-8') as f:
        body = json.load(f)
    print(body.get('message') or body.get('error') or '')
except Exception:
    print('')
PY
)"

  if [ "$ERROR_MSG" = "signature verification failed" ] && command -v openssl >/dev/null 2>&1; then
    python3 - <<'PY'
from pathlib import Path
Path('/tmp/agri_test_private.pem').write_text(
"""-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIBAhvHyy+MkYiKfJ6i80jbDZEzsDC8943UwQe5ZdPp+noAoGCCqGSM49
AwEHoUQDQgAES4hVNSi27fHAishx1nXki+lfFdhrSVKT4ubhb9IbG9Kj3NYu14Mm
VQKq13CS9jAYfnc/HDEzUHmJ9jSB3ZU2CA==
-----END EC PRIVATE KEY-----
""",
encoding='utf-8'
)
PY
    export SIGNATURE="$(printf '%s' "$HASH" | openssl dgst -sha256 -sign /tmp/agri_test_private.pem | xxd -p -c 4096 | tr -d '\n')"
    HTTP_CODE="$(send_ingest)"
  fi
fi

echo "HTTP $HTTP_CODE"
python3 - <<'PY'
import json
with open('/tmp/agri_ingest_response.json', 'r', encoding='utf-8') as f:
    print(json.dumps(json.load(f), indent=2))
PY
```

Expected outcome for a successful packet:

- HTTP status `202`
- response includes `accepted=true`
- response includes `recordId` and `receipt.txHash`

## 5) Query traceability and metrics endpoints

```bash
export TX_HASH="$(python3 - <<'PY'
import json
with open('/tmp/agri_ingest_response.json', 'r', encoding='utf-8') as f:
    body = json.load(f)
print(body.get('receipt', {}).get('txHash', ''))
PY
)"

curl -sS "http://127.0.0.1:8080/api/v1/devices/stm32-node-1/latest"
curl -sS "http://127.0.0.1:8080/api/v1/batches/BATCH-2026-0001/trace"
curl -sS "http://127.0.0.1:8080/api/v1/transactions/${TX_HASH}"
curl -sS "http://127.0.0.1:8080/api/v1/metrics/overview"
```

## 6) Optional realtime verification

If `websocat` is installed:

```bash
websocat ws://127.0.0.1:8080/ws/telemetry
websocat ws://127.0.0.1:8080/ws/alerts
```

While websocket clients are connected, repeat section 4 and confirm event types:

- accepted ingest -> `telemetry.ingested`
- rejected ingest -> `ingest.rejected`

## 7) Evidence files to archive per demo

- `/tmp/agri_ingest.json`
- `/tmp/agri_ingest_response.json`
- outputs from all query calls in section 5
- backend startup log (Terminal A)
