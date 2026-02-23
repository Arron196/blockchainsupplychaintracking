#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import statistics
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Measure ingest success rate and latency against backend-cpp.",
    )
    parser.add_argument("--base-url", default="http://127.0.0.1:8080", help="Gateway base URL")
    parser.add_argument("--requests", type=int, default=100, help="Measured request count")
    parser.add_argument(
        "--base-timestamp",
        type=int,
        default=1700002000,
        help="Base UNIX timestamp for deterministic payload generation",
    )
    parser.add_argument("--timeout-sec", type=float, default=3.0, help="HTTP timeout per request")
    parser.add_argument(
        "--output",
        default="tools/qa/artifacts/ingest_mock_baseline.json",
        help="Output JSON artifact path",
    )
    parser.add_argument(
        "--private-key",
        default="tools/qa/artifacts/keys/pubkey-1-test-private.pem",
        help="ECDSA private key path used for deterministic test workload",
    )
    return parser.parse_args()


def percentile(sorted_values: list[float], p: float) -> float:
    if not sorted_values:
        return 0.0
    position = (len(sorted_values) - 1) * (p / 100.0)
    low = int(math.floor(position))
    high = int(math.ceil(position))
    if low == high:
        return sorted_values[low]
    weight = position - low
    return sorted_values[low] * (1.0 - weight) + sorted_values[high] * weight


def sign_hash_hex(hash_hex: str, private_key_path: Path) -> str:
    if subprocess.run(["openssl", "version"], capture_output=True, check=False).returncode != 0:
        raise RuntimeError("openssl command is unavailable or not functional")

    # Default mode keeps parity with current backend contract where the hash hex text
    # itself is passed into DigestSign/DigestVerify. A protocol upgrade can switch to
    # raw hash bytes on both producer and verifier together.
    result = subprocess.run(
        ["openssl", "dgst", "-sha256", "-sign", str(private_key_path), "-binary"],
        input=hash_hex.encode("utf-8"),
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        stderr_text = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"openssl signing failed: {stderr_text}")
    return result.stdout.hex()


def build_packet(index: int, base_timestamp: int, private_key_path: Path) -> dict[str, Any]:
    telemetry = {
        "humidityPct": 56 + (index % 7),
        "soilMoisturePct": 42 + (index % 5),
        "temperatureC": round(21.5 + (index % 9) * 0.2, 1),
    }
    telemetry_json = json.dumps(telemetry, separators=(",", ":"), sort_keys=True)
    device_id = f"stm32-node-{(index % 4) + 1}"
    timestamp = base_timestamp + index
    canonical = f"{device_id}|{timestamp}|{telemetry_json}"
    hash_hex = hashlib.sha256(canonical.encode("utf-8")).hexdigest()

    signature = sign_hash_hex(hash_hex, private_key_path)

    return {
        "deviceId": device_id,
        "timestamp": timestamp,
        "telemetry": telemetry,
        "hash": hash_hex,
        "signature": signature,
        "pubKeyId": "pubkey-1",
        "transport": "wifi",
        "batchCode": "BENCHMARK-QA-M1",
    }


def post_json(url: str, payload: dict[str, Any], timeout_sec: float) -> tuple[int, dict[str, Any], float]:
    data = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    start_ns = time.perf_counter_ns()
    try:
        with urllib.request.urlopen(request, timeout=timeout_sec) as response:
            body_text = response.read().decode("utf-8")
            status = int(response.status)
    except urllib.error.HTTPError as exc:
        status = int(exc.code)
        body_text = exc.read().decode("utf-8", errors="replace")
    except urllib.error.URLError as exc:
        elapsed_ms = (time.perf_counter_ns() - start_ns) / 1_000_000.0
        return 0, {"error": str(exc.reason)}, elapsed_ms

    elapsed_ms = (time.perf_counter_ns() - start_ns) / 1_000_000.0
    try:
        parsed = json.loads(body_text)
        if not isinstance(parsed, dict):
            parsed = {"raw": body_text}
    except json.JSONDecodeError:
        parsed = {"raw": body_text}
    return status, parsed, elapsed_ms


def get_json(url: str, timeout_sec: float) -> dict[str, Any]:
    request = urllib.request.Request(url, method="GET")
    with urllib.request.urlopen(request, timeout=timeout_sec) as response:
        body_text = response.read().decode("utf-8")
    parsed = json.loads(body_text)
    return parsed if isinstance(parsed, dict) else {"raw": body_text}


def main() -> int:
    args = parse_args()
    if args.requests <= 0:
        raise ValueError("--requests must be > 0")

    private_key_path = Path(args.private_key)
    if not private_key_path.exists():
        raise FileNotFoundError(f"private key not found: {private_key_path}")

    ingest_url = f"{args.base_url.rstrip('/')}/api/v1/ingest"
    metrics_url = f"{args.base_url.rstrip('/')}/api/v1/metrics/overview"

    per_request: list[dict[str, Any]] = []
    latencies: list[float] = []
    accepted_count = 0

    for index in range(args.requests):
        payload = build_packet(index, args.base_timestamp, private_key_path)
        status, response_json, latency_ms = post_json(ingest_url, payload, args.timeout_sec)
        accepted = status == 202 and response_json.get("accepted") is True
        if accepted:
            accepted_count += 1
        latencies.append(latency_ms)
        per_request.append(
            {
                "index": index,
                "status": status,
                "accepted": accepted,
                "latencyMs": round(latency_ms, 3),
                "message": response_json.get("message", response_json.get("error", "")),
            }
        )

    sorted_latencies = sorted(latencies)
    rejected_count = args.requests - accepted_count
    success_rate = accepted_count / args.requests

    server_metrics: dict[str, Any]
    try:
        server_metrics = get_json(metrics_url, args.timeout_sec)
    except Exception as exc:
        server_metrics = {"error": str(exc)}

    artifact = {
        "benchmark": "backend-cpp ingest success-rate/latency baseline",
        "mode": "mock",
        "deterministicWorkload": {
            "requests": args.requests,
            "baseTimestamp": args.base_timestamp,
            "pubKeyId": "pubkey-1",
            "batchCode": "BENCHMARK-QA-M1",
        },
        "clientResults": {
            "totalRequests": args.requests,
            "acceptedRequests": accepted_count,
            "rejectedRequests": rejected_count,
            "successRate": round(success_rate, 6),
            "latencyMs": {
                "min": round(min(sorted_latencies), 3),
                "max": round(max(sorted_latencies), 3),
                "avg": round(statistics.fmean(latencies), 3),
                "p50": round(percentile(sorted_latencies, 50.0), 3),
                "p95": round(percentile(sorted_latencies, 95.0), 3),
            },
        },
        "serverMetrics": server_metrics,
        "perRequest": per_request,
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(artifact, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")

    print(f"artifact: {output_path}")
    print(
        "successRate={:.2%} accepted={} rejected={} avgMs={} p95Ms={}".format(
            success_rate,
            accepted_count,
            rejected_count,
            artifact["clientResults"]["latencyMs"]["avg"],
            artifact["clientResults"]["latencyMs"]["p95"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
