#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
sha256sum -c --quiet artifact/SHA256SUMS
echo 'Frozen source, benchmarks, configs, and reference data: hashes PASS'
