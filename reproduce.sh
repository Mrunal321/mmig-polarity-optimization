#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

DOCKER=0
VERIFY_ONLY=0
SKIP_RUNTIME=0
SMOKE=0
for arg in "$@"; do
  case "$arg" in
    --docker) DOCKER=1 ;;
    --verify-only) VERIFY_ONLY=1 ;;
    --skip-runtime) SKIP_RUNTIME=1 ;;
    --smoke) SMOKE=1 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

if [[ "$DOCKER" == 1 ]]; then
  docker build -t mmig-esl-artifact:local .
  args=()
  [[ "$VERIFY_ONLY" == 1 ]] && args+=(--verify-only)
  [[ "$SKIP_RUNTIME" == 1 ]] && args+=(--skip-runtime)
  [[ "$SMOKE" == 1 ]] && args+=(--smoke)
  docker run --rm --user "$(id -u):$(id -g)" -e MMIG_IN_DOCKER=1 \
    -v "$ROOT:/workspace" -w /workspace mmig-esl-artifact:local \
    ./reproduce.sh "${args[@]}"
  exit
fi

required=(git g++ cmake make python3 sha256sum)
if [[ "$SMOKE" != 1 ]]; then
  required+=(latexmk pdfinfo rg)
fi
for tool in "${required[@]}"; do
  command -v "$tool" >/dev/null || {
    echo "Missing required tool: $tool. Use ./reproduce.sh --docker." >&2
    exit 1
  }
done

mkdir -p results/generated logs/generated
export MPLCONFIGDIR="$ROOT/results/generated/matplotlib_cache"
mkdir -p "$MPLCONFIGDIR"
started="$(date +%s)"
echo '[0/16] Environment'
uname -a
g++ --version | head -1
python3 --version
echo '[1/16] Build pinned ABC and frozen source'
./scripts/build.sh > logs/generated/build.log 2>&1 || {
  tail -70 logs/generated/build.log >&2; exit 1;
}
echo '[2/16] Validate frozen inputs and reference hashes'
./scripts/check_hashes.sh
python3 scripts/test_cec.py

if [[ "$SMOKE" == 1 ]]; then
  echo '[3/16] One-circuit T/P/M, CEC, LUT6 and direct MIN-lowering smoke test'
  python3 scripts/run_experiment.py --benchmarks adder --skip-runtime
  echo 'SMOKE REPRODUCTION COMPLETE'
  exit
fi

if [[ "$VERIFY_ONLY" == 1 ]]; then
  echo '[3-10/16] Verify committed reference networks, CEC and common LUT6 map'
  python3 scripts/run_experiment.py --verify-only
else
  echo '[3-9/16] Rerun T/P/M and direct MIN lowering with raw ABC CEC'
  args=()
  [[ "$SKIP_RUNTIME" == 1 ]] && args+=(--skip-runtime)
  python3 scripts/run_experiment.py "${args[@]}"
fi

echo '[11/16] Calculate and verify deterministic paper numbers'
python3 scripts/verify_paper_numbers.py
echo '[12/16] Regenerate LaTeX tables'
python3 scripts/generate_tables.py
echo '[13/16] Regenerate vector and raster figure'
python3 scripts/generate_figures.py
echo '[14/16] Compile manuscript'
(cd paper && latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex \
  > ../logs/generated/latexmk.log 2>&1)
pages="$(pdfinfo paper/main.pdf | awk '/^Pages:/ {print $2}')"
[[ "$pages" == 4 ]] || { echo "Expected 4 pages; found $pages" >&2; exit 1; }
if rg -i 'undefined (citation|reference)|overfull \\hbox|file .* not found' paper/main.log; then
  echo 'LaTeX warning requires inspection' >&2
  exit 1
fi
sha256sum paper/main.pdf results/generated/three_flow_results.csv \
  results/generated/min_lowering.csv results/generated/paper_numbers.json \
  > results/generated/SHA256SUMS
echo '[15/16] Results and paper checks passed'
python3 - <<'PY'
import json
from pathlib import Path
x=json.loads(Path('results/generated/paper_numbers.json').read_text())
for flow in ('T','P','M'):
    r=x['totals'][flow]
    print(f'{flow}: G={r["G"]}, I_nonconst={r["I_nonconst"]}, LUT6={r["LUT6"]}')
print('M vs P:', x['M_vs_P_pct'])
print('MIN lowering:', x['lowering_I'])
print(f'CEC: {x["cec_flow"]+x["cec_lowering"]+x["cec_start"]+x["cec_m_stage1"]} '
      'start/Stage-1/final/ablation PASS, 0 FAIL')
print('Paper: paper/main.pdf, 4 pages')
PY
echo "Elapsed: $(($(date +%s)-started)) s"
echo 'REPRODUCTION COMPLETE'
