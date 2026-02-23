#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BACKEND_DIR="${REPO_ROOT}/backend-cpp"
BUILD_DIR="${BACKEND_DIR}/build-qa-benchmark"
ARTIFACT_DIR="${SCRIPT_DIR}/artifacts"
SQLITE_PATH="${ARTIFACT_DIR}/agri_gateway_benchmark.db"
SERVER_LOG="${ARTIFACT_DIR}/agri_gateway_benchmark.log"
KEY_DIR="${ARTIFACT_DIR}/keys"
PUBLIC_KEY_DIR="${KEY_DIR}/public"
PUBLIC_KEY_PATH="${PUBLIC_KEY_DIR}/pubkey-1.pem"
PRIVATE_KEY_PATH="${KEY_DIR}/pubkey-1-test-private.pem"

OUTPUT_PATH="${ARTIFACT_DIR}/ingest_mock_baseline.json"
POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --output)
      if [[ -z "${2:-}" ]]; then
        echo "--output requires a path" >&2
        exit 2
      fi
      OUTPUT_PATH="$2"
      shift 2
      ;;
    *)
      POSITIONAL_ARGS+=("$1")
      shift
      ;;
  esac
done
set -- "${POSITIONAL_ARGS[@]}"

mkdir -p "${ARTIFACT_DIR}"
mkdir -p "${KEY_DIR}" "${PUBLIC_KEY_DIR}"
mkdir -p "$(dirname "${OUTPUT_PATH}")"
rm -f "${SQLITE_PATH}" "${SERVER_LOG}" "${OUTPUT_PATH}"

if ! command -v openssl >/dev/null 2>&1; then
  echo "openssl is required to generate benchmark key material" >&2
  exit 1
fi

openssl ecparam -name prime256v1 -genkey -noout -out "${PRIVATE_KEY_PATH}"
openssl ec -in "${PRIVATE_KEY_PATH}" -pubout -out "${PUBLIC_KEY_PATH}"

cmake -S "${BACKEND_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=FALSE -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "${BUILD_DIR}" --config Release --target agri_gateway

if ! grep -q -- "-DAGRI_USE_OPENSSL=1" "${BUILD_DIR}/compile_commands.json"; then
  echo "backend-cpp must be built with OpenSSL support for benchmark signing" >&2
  exit 1
fi

AGRI_CHAIN_MODE=mock \
AGRI_SQLITE_PATH="${SQLITE_PATH}" \
AGRI_PUBLIC_KEYS_DIR="${PUBLIC_KEY_DIR}" \
"${BUILD_DIR}/agri_gateway" >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

cleanup() {
  if kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

for _ in $(seq 1 80); do
  if curl --silent --fail "http://127.0.0.1:8080/health" >/dev/null; then
    break
  fi
  sleep 0.25
done

if ! curl --silent --fail "http://127.0.0.1:8080/health" >/dev/null; then
  echo "gateway did not become healthy; see ${SERVER_LOG}" >&2
  exit 1
fi

python3 "${SCRIPT_DIR}/ingest_benchmark.py" \
  --base-url "http://127.0.0.1:8080" \
  --private-key "${PRIVATE_KEY_PATH}" \
  --output "${OUTPUT_PATH}" \
  "$@"

echo "server log: ${SERVER_LOG}"
