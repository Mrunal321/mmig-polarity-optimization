#!/usr/bin/env python3
"""Generate manuscript tables from the generated deterministic CSV files."""

import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HERE = ROOT / 'paper'
CONTROL = ROOT / 'results/generated'
REFERENCE = ROOT / 'results/reference'
ORDER = ('adder', 'arbiter', 'bar', 'cavlc', 'ctrl', 'dec', 'i2c',
         'int2float', 'max', 'priority', 'router', 'voter')


def read_csv(name):
    with (CONTROL / name).open(newline='') as f:
        return list(csv.DictReader(f))


def load():
    rows = read_csv('three_flow_results.csv')
    assert len(rows) == 36
    data = {}
    for row in rows:
        key = row['benchmark'], row['flow']
        assert key not in data and row['flow'] in ('T', 'P', 'M')
        assert row['ABC_CEC'] == 'True', key
        assert int(row['G']) == int(row['MAJ_count']) + int(row['MIN_count']), key
        data[key] = row
    for name in ORDER:
        t, p, m = (data[name, flow] for flow in ('T', 'P', 'M'))
        assert t['start_sha256'] == p['start_sha256'] == m['start_sha256'], name
        assert t['source_sha256'] == p['source_sha256'] == m['source_sha256'], name
    assert len(data) == 36
    totals = {flow: {metric: sum(float(data[name, flow][metric]) for name in ORDER)
                     for metric in ('G', 'I_nonconst', 'LUT6')}
              for flow in ('T', 'P', 'M')}
    # The manuscript reports the archived machine timings. Current-machine
    # timings remain in generated CSV and must not silently replace them.
    archived = {(r['benchmark'], r['flow']): r for r in
                csv.DictReader((REFERENCE / 'three_flow_results.csv').open())}
    for flow in ('T', 'P', 'M'):
        totals[flow]['runtime_s'] = sum(float(archived[name, flow]['runtime_s'])
                                        for name in ORDER)
    for flow, expected in {'T': (15454, 6749, 5863),
                           'P': (15205, 6501, 5815),
                           'M': (15070, 6021, 5867)}.items():
        assert tuple(round(totals[flow][metric]) for metric in ('G', 'I_nonconst', 'LUT6')) == expected
    assert round(totals['T']['runtime_s'], 2) == 0.33
    assert round(totals['P']['runtime_s'], 2) == 24.08
    assert round(totals['M']['runtime_s'], 2) == 239.87
    lower = read_csv('min_lowering.csv')
    assert sum(int(r['I_nonconst']) for r in lower if r['stage'] == 'lowered_phase') == 6668
    assert sum(int(r['I_nonconst']) for r in lower if r['stage'] == 'native') == 6021
    assert sum(int(r['I_nonconst']) for r in lower if r['stage'] == 'lowered_raw') == 7028
    manifest = {r['benchmark']: r for r in
                csv.DictReader((ROOT / 'benchmarks/MANIFEST.csv').open())}
    assert set(manifest) == set(ORDER)
    for name in ORDER:
        assert manifest[name]['starting_mig_sha256'] == data[name, 'M']['start_sha256']
    return data, totals


def pct(new, old):
    return 100 * (new - old) / old


def signed(value):
    if abs(value) < 0.005:
        return '0.00'
    return f'{value:+.2f}'


def main():
    data, totals = load()
    out = HERE / 'tables'
    out.mkdir(exist_ok=True)
    lines = [
        r'\begin{table*}[t]',
        r'\caption{Matched structural results. $P$ is the pure-MIG control and $M$ is the frozen mMIG flow. $G$ counts MAJ+MIN nodes; $I$ counts complemented nonconstant graph connections. Negative changes favor $M$.}',
        r'\label{tab:primary}',
        r'\centering',
        r'\begin{tabular}{lrrrrrrr}',
        r'\toprule',
        r'Circuit & $P:G$ & $M:G$ & $\Delta G$ (\%) & $P:I$ & $M:I$ & $\Delta I$ (\%) & MIN \\',
        r'\midrule',
    ]
    for name in ORDER:
        p, m = data[name, 'P'], data[name, 'M']
        gp, gm = int(p['G']), int(m['G'])
        ip, im = int(p['I_nonconst']), int(m['I_nonconst'])
        lines.append(f'{name} & {gp} & {gm} & {signed(pct(gm, gp))} & '
                     f'{ip} & {im} & {signed(pct(im, ip))} & {m["MIN_count"]} ' + r'\\')
    pg, mg = (int(totals[f]['G']) for f in ('P', 'M'))
    pi, mi = (int(totals[f]['I_nonconst']) for f in ('P', 'M'))
    lines += [r'\midrule',
              f'Sum & {pg} & {mg} & {signed(pct(mg, pg))} & '
              f'{pi} & {mi} & {signed(pct(mi, pi))} & --- ' + r'\\',
              r'\bottomrule', r'\end{tabular}', r'\end{table*}']
    (out / 'primary_pm_table.tex').write_text('% AUTO-GENERATED -- DO NOT EDIT\n' + '\n'.join(lines) + '\n')

    lines = [
        r'\begin{table}[t]',
        r'\caption{Aggregate context for the three fixed flows.}',
        r'\label{tab:summary}',
        r'\centering',
        r'\begin{tabular}{lrrrr}',
        r'\toprule',
        r'Flow & $G$ & $I$ & LUT6 & Time (s) \\',
        r'\midrule',
    ]
    for flow in ('T', 'P', 'M'):
        x = totals[flow]
        lines.append(f'{flow} & {int(x["G"])} & {int(x["I_nonconst"])} & '
                     f'{int(x["LUT6"])} & {x["runtime_s"]:.2f} ' + r'\\')
    lines += [r'\bottomrule', r'\end{tabular}', r'\end{table}']
    (out / 'summary_table.tex').write_text('% AUTO-GENERATED -- DO NOT EDIT\n' + '\n'.join(lines) + '\n')

    lower = read_csv('min_lowering.csv')
    lower_totals = {stage: {metric: sum(int(r[metric]) for r in lower
                                            if r['stage'] == stage)
                            for metric in ('G', 'I_nonconst', 'MIN_count')}
                    for stage in ('native', 'lowered_raw', 'lowered_phase')}
    for x in lower_totals.values():
        assert x['G'] == 15070
    assert lower_totals['native']['MIN_count'] == 616
    assert lower_totals['lowered_raw']['MIN_count'] == 0
    assert lower_totals['lowered_phase']['MIN_count'] == 0
    lines = [
        r'\begin{table}[t]',
        r'\caption{Direct MIN-lowering ablation across twelve circuits. The three $M$ rows share gate-incidence topology.}',
        r'\label{tab:lowering}',
        r'\centering',
        r'\begin{tabular}{lrrr}',
        r'\toprule',
        r'Representation & $G$ & $I$ & MIN \\',
        r'\midrule',
    ]
    for label, stage in (('$M$ native', 'native'),
                         ('$M$ MAJ-only raw', 'lowered_raw'),
                         ('$M$ MAJ-only + phase', 'lowered_phase')):
        x = lower_totals[stage]
        lines.append(f'{label} & {x["G"]} & {x["I_nonconst"]} & '
                     f'{x["MIN_count"]} ' + r'\\')
    x = totals['P']
    lines += [f'$P$ pure MIG & {int(x["G"])} & {int(x["I_nonconst"])} & 0 ' + r'\\',
              r'\bottomrule', r'\end{tabular}', r'\end{table}']
    (out / 'lowering_table.tex').write_text('% AUTO-GENERATED -- DO NOT EDIT\n' + '\n'.join(lines) + '\n')
    (out / 'generated_metrics.json').write_text(json.dumps(totals, indent=2) + '\n')
    print('Wrote primary, summary, and lowering tables from audited CSV rows.')


if __name__ == '__main__':
    main()
