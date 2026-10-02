#!/usr/bin/env python3
"""Fail on deterministic result drift; retain current-machine timing separately."""

import json
from pathlib import Path

from common import ROOT, POSITIVE, NEGATIVE, read_csv

CLAIMS = json.loads((ROOT / 'paper/paper_claims.json').read_text())
DATA = ROOT / 'results/generated'


def transcript_passes(path):
    path = Path(path)
    if not path.is_file():
        return False
    raw = path.read_text()
    return (raw.count('RETURN_CODE: 0') == 3 and bool(POSITIVE.search(raw))
            and not NEGATIVE.search(raw))


rows = read_csv(DATA / 'three_flow_results.csv')
lower = read_csv(DATA / 'min_lowering.csv')
if len(rows) != 36 or len(lower) != 60:
    raise SystemExit(f'incomplete results: {len(rows)} flow rows, {len(lower)} ablation rows')
by = {(r['benchmark'], r['flow']): r for r in rows}
if len(by) != 36:
    raise SystemExit('duplicate benchmark/flow rows')
totals = {flow: {metric: sum(int(r[metric]) for r in rows if r['flow'] == flow)
                 for metric in ('G', 'I_nonconst', 'LUT6')}
          for flow in ('T', 'P', 'M')}
for flow, metrics in CLAIMS['flows'].items():
    for metric, expected in metrics.items():
        if totals[flow][metric] != expected:
            raise SystemExit(f'{flow} {metric}: {totals[flow][metric]} != {expected}')
ltotals = {stage: sum(int(r['I_nonconst']) for r in lower if r['stage'] == stage)
           for stage in ('native', 'lowered_raw', 'lowered_phase')}
if ltotals != CLAIMS['lowering_I']:
    raise SystemExit(f'lowering drift: {ltotals}')
native_min = sum(int(r['MIN_count']) for r in lower if r['stage'] == 'native')
if native_min != CLAIMS['native_MIN']:
    raise SystemExit(f'native MIN drift: {native_min}')

names = sorted({r['benchmark'] for r in rows})
counts = {}
for metric in ('G', 'I_nonconst'):
    signs = [int(by[n, 'M'][metric]) - int(by[n, 'P'][metric]) for n in names]
    counts[metric] = [sum(x < 0 for x in signs), sum(x == 0 for x in signs),
                      sum(x > 0 for x in signs)]
    if counts[metric] != CLAIMS['M_vs_P_counts'][metric]:
        raise SystemExit(f'{metric} better/tie/worse drift: {counts[metric]}')

percent = lambda new, old: round(100 * (new - old) / old, 2)
for metric in ('G', 'I_nonconst'):
    actual = percent(totals['M'][metric], totals['P'][metric])
    if actual != CLAIMS['M_vs_P_pct'][metric]:
        raise SystemExit(f'M vs P {metric} drift: {actual}')
    actual_t = percent(totals['M'][metric], totals['T'][metric])
    if actual_t != CLAIMS['M_vs_T_pct'][metric]:
        raise SystemExit(f'M vs T {metric} drift: {actual_t}')
lowered_vs_p = percent(ltotals['lowered_phase'], totals['P']['I_nonconst'])
if lowered_vs_p != CLAIMS['lowered_phase_vs_P_I_pct']:
    raise SystemExit(f'lowered M + phase vs P drift: {lowered_vs_p}')
for r in rows:
    if r['ABC_CEC'] != 'True' or not transcript_passes(ROOT / r['cec_transcript']):
        raise SystemExit(f'CEC missing or unpassed: {r["benchmark"]}/{r["flow"]}')
for r in lower:
    if r['cec_pass'] != 'True' or not transcript_passes(ROOT / r['cec_transcript']):
        raise SystemExit(f'lowering CEC missing: {r["benchmark"]}/{r["stage"]}')
for name in names:
    if not transcript_passes(DATA / 'cec/start' / f'{name}.cec.txt'):
        raise SystemExit(f'starting MIG CEC missing: {name}')
    stage1_paths = (DATA / 'cec/intermediate' / f'{name}_M_gate.cec.txt',
                    DATA / 'cec/reference/m_stage1' / f'{name}.cec.txt')
    if not any(transcript_passes(path) for path in stage1_paths):
        raise SystemExit(f'M Stage 1 CEC missing: {name}')

summary = {'totals': totals, 'M_vs_P_pct': CLAIMS['M_vs_P_pct'],
           'M_vs_P_counts': counts, 'lowering_I': ltotals,
           'cec_flow': len(rows), 'cec_lowering': len(lower),
           'cec_start': len(names), 'cec_m_stage1': len(names)}
(DATA / 'paper_numbers.json').write_text(json.dumps(summary, indent=2) + '\n')
(DATA / 'three_flow_summary.md').write_text(
    '# Generated fixed-flow summary\n\n'
    + '\n'.join(f'- {f}: G={totals[f]["G"]}, I={totals[f]["I_nonconst"]}, '
                f'LUT6={totals[f]["LUT6"]}' for f in ('T', 'P', 'M'))
    + f'\n- M vs P: G {CLAIMS["M_vs_P_pct"]["G"]:+.2f}%, '
      f'I {CLAIMS["M_vs_P_pct"]["I_nonconst"]:+.2f}%\n')
print('Deterministic paper claims: PASS')
