#!/usr/bin/env python3
"""Rerun the frozen T/P/M control, direct MIN ablation and time sensitivity."""

import argparse
import json
from pathlib import Path

from common import (ROOT, GENERATED, BENCHES, cec, execute, map_lut6,
                    parse_metrics, read_csv, relative, sha, write_csv)
from topology import topology


def configuration(name):
    return json.loads((ROOT / 'configs' / f'{name}.json').read_text())


def checked_input():
    rows = read_csv(ROOT / 'benchmarks/MANIFEST.csv')
    assert {r['benchmark'] for r in rows} == set(BENCHES)
    for row in rows:
        for path_key, hash_key in (('original_blif', 'original_sha256'),
                                   ('starting_mig', 'starting_mig_sha256')):
            if sha(ROOT / row[path_key]) != row[hash_key]:
                raise RuntimeError(f'{row["benchmark"]}: {path_key} hash drift')
    return {r['benchmark']: r for r in rows}


def abc_path():
    path = ROOT / 'build/abc_path.txt'
    if not path.exists():
        raise RuntimeError('Build first: ./scripts/build.sh')
    abc = Path(path.read_text().strip())
    if not abc.is_file():
        raise RuntimeError(f'ABC not found: {abc}')
    return abc


def synth(source, prefix, flags, section, label, original, abc, expected_min=None):
    prefix.parent.mkdir(parents=True, exist_ok=True)
    binary = ROOT / 'build/blif2mig_paper'
    cmd = [relative(binary), relative(source), relative(prefix), *flags]
    rc, seconds, output = execute(cmd, Path(str(prefix) + '.log'), 900)
    suffix = '_mmig_opt.blif' if section == 'Optimized mMIG' else '_maj_opt.blif'
    result = Path(str(prefix) + suffix)
    if rc or not result.is_file():
        raise RuntimeError(f'{label}: synthesis failed: {prefix}.log')
    metrics = parse_metrics(output, section)
    if expected_min is not None and (metrics['MIN_count'] > 0) != expected_min:
        raise RuntimeError(f'{label}: unexpected MIN count')
    transcript = cec(abc, original, result,
                     GENERATED / 'cec/intermediate' / (label + '.cec.txt'))
    return {'path': result, 'sha256': sha(result), 'runtime_s': seconds,
            'command': cmd, 'cec_transcript': relative(transcript), **metrics}


def better(after, before):
    return (after['G'], after['I_nonconst'], after['D']) < (before['G'], before['I_nonconst'], before['D'])


def admissible(after, before):
    if after['G'] < before['G']:
        return after['I_nonconst'] <= before['I_nonconst'] + 1000
    return after['G'] == before['G'] and after['I_nonconst'] <= before['I_nonconst']


def run_p(start, original, abc, folder, bench):
    cfg = configuration('matched_mig')
    stages = []
    pre = synth(start, folder / 'pre', cfg['pre'], 'Optimized MIG',
                f'{bench}_P_pre', original, abc, False)
    stages.append(('pre', pre, True))
    current = pre
    for round_no in (1, 2):
        flags = cfg['round'] + (['--mig-enable-exact'] if current['G'] <= 3500 else [])
        trial = synth(current['path'], folder / f'round{round_no}', flags,
                      'Optimized MIG', f'{bench}_P_round{round_no}', original, abc, False)
        accepted = admissible(trial, current) and better(trial, current)
        stages.append((f'round{round_no}', trial, accepted))
        if not accepted:
            break
        current = trial
    final = synth(current['path'], folder / 'phase', cfg['phase'],
                  'Optimized MIG', f'{bench}_P_phase', original, abc, False)
    stages.append(('phase', final, True))
    return final, stages


def flow_row(bench, seed, flow, start_hash, source_hash, result, abc):
    dest = GENERATED / 'mapped' / flow / f'{bench}.blif'
    lut, levels, map_seconds = map_lut6(abc, result['path'], dest,
                                         GENERATED / 'logs/map' / flow / f'{bench}.log')
    final_cec = cec(abc, ROOT / 'benchmarks/epfl/original' / f'{bench}.blif',
                    result['path'], GENERATED / 'cec' / flow / f'{bench}.cec.txt')
    return {'benchmark': bench, 'seed': seed, 'start_sha256': start_hash,
            'source_sha256': source_hash, 'flow': flow,
            **{key: result[key] for key in ('G', 'D', 'I_raw', 'I_nonconst',
                                            'MAJ_count', 'MIN_count')},
            'total_edges': 3 * result['G'] + result['pos'],
            'LUT6': lut, 'LUT_depth': levels,
            'runtime_s': result['runtime_s'], 'ABC_CEC': True,
            'output_path': relative(result['path']),
            'output_sha256': result['sha256'],
            'cec_transcript': relative(final_cec),
            'mapping_runtime_s': map_seconds}


def verify_reference_row(generated, reference):
    for key in ('G', 'D', 'I_raw', 'I_nonconst', 'MAJ_count', 'MIN_count',
                'total_edges', 'LUT6', 'LUT_depth', 'output_sha256'):
        if str(generated[key]) != str(reference[key]):
            raise RuntimeError(f'{generated["benchmark"]}/{generated["flow"]} '
                               f'{key}: generated {generated[key]} != reference {reference[key]}')


def lower_min(bench, gate, native, original, abc):
    folder = GENERATED / 'runs' / bench / 'lowering'
    folder.mkdir(parents=True, exist_ok=True)
    prefix = folder / 'diag'
    cmd = [relative(ROOT / 'build/phase_audit'), relative(gate), relative(prefix)]
    rc, seconds, output = execute(cmd, folder / 'diag.log', 900)
    if rc:
        raise RuntimeError(f'{bench}: phase_audit failed')
    rows = []
    expected = topology(gate)['topology_hash']
    for stage, section in (('imported', 'Imported Stage 1'),
                           ('mixed', None), ('native', 'Native Stage 2'),
                           ('lowered_raw', 'Lowered raw'),
                           ('lowered_phase', 'Lowered phase')):
        path = Path(str(prefix) + f'.{stage}.blif')
        metrics = parse_metrics(output, section) if section else {}
        transcript = cec(abc, original, path,
                         GENERATED / 'cec/lowering' / stage / f'{bench}.cec.txt')
        top = topology(path)
        if top['topology_hash'] != expected:
            raise RuntimeError(f'{bench}/{stage}: gate incidence topology changed')
        rows.append({'benchmark': bench, 'stage': stage,
                     **{key: metrics.get(key, '') for key in
                        ('G', 'D', 'I_raw', 'I_nonconst', 'MAJ_count', 'MIN_count')},
                     'topology_hash': top['topology_hash'],
                     'gate_type_hash': sha(Path(str(prefix) + f'.{stage}.types')),
                     'cec_pass': True, 'cec_transcript': relative(transcript),
                     'path': relative(path), 'sha256': sha(path),
                     'diagnostic_runtime_s': seconds,
                     'diagnostic_command': json.dumps(cmd)})
    if next(row for row in rows if row['stage'] == 'native')['sha256'] != sha(native):
        raise RuntimeError(f'{bench}: native phase diagnostic differs from M')
    return rows


def runtime_sensitivity(bench, pre, m_runtime, original, abc):
    """Secondary machine-dependent P search; never used for fixed primary tables."""
    cfg = configuration('matched_mig')
    folder = GENERATED / 'runs' / bench / 'P_TIME'
    current = pre
    best = pre
    spent = pre['runtime_s']
    attempts = []
    for number in range(1, 21):
        if spent >= m_runtime:
            break
        flags = cfg['round'] + (['--mig-enable-exact'] if current['G'] <= 3500 else [])
        trial = synth(current['path'], folder / f'round{number:02d}', flags,
                      'Optimized MIG', f'{bench}_P_TIME_{number:02d}', original, abc, False)
        spent += trial['runtime_s']
        ok = admissible(trial, current)
        if ok and trial['sha256'] != current['sha256']:
            current = trial
        if ok and better(trial, best):
            best = trial
        attempts.append({'benchmark': bench, 'round': number, 'G': trial['G'],
                         'I_nonconst': trial['I_nonconst'], 'admissible': ok,
                         'elapsed_s': spent, 'budget_s': m_runtime,
                         'path': relative(trial['path'])})
    final = synth(best['path'], folder / 'phase', cfg['phase'],
                  'Optimized MIG', f'{bench}_P_TIME_phase', original, abc, False)
    spent += final['runtime_s']
    lut, depth, _ = map_lut6(abc, final['path'], folder / 'mapped.blif', folder / 'map.log')
    return {'benchmark': bench, 'rounds_attempted': len(attempts),
            'target_m_runtime_s': m_runtime, 'p_time_runtime_s': spent,
            'budget_reached': spent >= m_runtime, 'G': final['G'], 'D': final['D'],
            'I_raw': final['I_raw'], 'I_nonconst': final['I_nonconst'],
            'MAJ_count': final['MAJ_count'], 'MIN_count': 0,
            'LUT6': lut, 'LUT_depth': depth, 'ABC_CEC': True,
            'output_path': relative(final['path']), 'sha256': final['sha256'],
            'cec_transcript': final['cec_transcript']}, attempts


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--benchmarks', nargs='*', choices=BENCHES, default=list(BENCHES))
    parser.add_argument('--skip-runtime', action='store_true')
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    manifest = checked_input()
    abc = abc_path()
    reference = {(r['benchmark'], r['flow']): r for r in
                 read_csv(ROOT / 'results/reference/three_flow_results.csv')}
    lower_reference = {(r['benchmark'], r['stage']): r for r in
                       read_csv(ROOT / 'results/reference/lowered_mmig_results.csv')}
    stage1_reference = {r['benchmark']: r for r in
                        read_csv(ROOT / 'results/reference/m_stage1_hashes.csv')}
    tcfg, mcfg = configuration('testa_style'), configuration('zg2_frozen')
    rows, lower_rows, runtime_rows, attempts, stage_rows = [], [], [], [], []
    for bench in args.benchmarks:
        item = manifest[bench]
        seed = item['selected_seed']
        original = ROOT / item['original_blif']
        start = ROOT / item['starting_mig']
        cec(abc, original, start, GENERATED / 'cec/start' / f'{bench}.cec.txt')
        if args.verify_only:
            s1 = stage1_reference[bench]
            if sha(ROOT / s1['stage1_blif']) != s1['sha256']:
                raise RuntimeError(f'{bench}: reference M Stage 1 hash drift')
            cec(abc, original, ROOT / s1['stage1_blif'],
                GENERATED / 'cec/reference/m_stage1' / f'{bench}.cec.txt')
            for flow in ('T', 'P', 'M'):
                ref = reference[bench, flow]
                network = ROOT / ref['output_path']
                if sha(network) != ref['output_sha256']:
                    raise RuntimeError(f'{bench}/{flow}: reference network hash drift')
                cec(abc, original, network,
                    GENERATED / 'cec' / flow / f'{bench}.cec.txt')
                lut, depth, _ = map_lut6(abc, network,
                    GENERATED / 'mapped/reference' / flow / f'{bench}.blif',
                    GENERATED / 'logs/map/reference' / flow / f'{bench}.log')
                if (lut, depth) != (int(ref['LUT6']), int(ref['LUT_depth'])):
                    raise RuntimeError(f'{bench}/{flow}: reference mapper drift')
            for stage in ('imported', 'mixed', 'native', 'lowered_raw', 'lowered_phase'):
                lr = lower_reference[bench, stage]
                network = ROOT / lr['path']
                if sha(network) != lr['sha256']:
                    raise RuntimeError(f'{bench}/{stage}: lowering hash drift')
                cec(abc, original, network,
                    GENERATED / 'cec/lowering' / stage / f'{bench}.cec.txt')
            print(f'{bench}: reference hashes, CEC and LUT6 PASS', flush=True)
            continue
        folder = GENERATED / 'runs' / bench
        t = synth(start, folder / 'T/t', tcfg['flags'], 'Optimized MIG',
                  f'{bench}_T', original, abc, False)
        trow = flow_row(bench, seed, 'T', sha(start), sha(original), t, abc)
        verify_reference_row(trow, reference[bench, 'T'])
        p, stages = run_p(start, original, abc, folder / 'P', bench)
        p['runtime_s'] = sum(s['runtime_s'] for _, s, _ in stages)
        for label, stage, selected in stages:
            stage_rows.append({'benchmark': bench, 'stage': label, 'selected': selected,
                               'G': stage['G'], 'I_nonconst': stage['I_nonconst'],
                               'runtime_s': stage['runtime_s'],
                               'path': relative(stage['path']), 'sha256': stage['sha256']})
        prow = flow_row(bench, seed, 'P', sha(start), sha(original), p, abc)
        verify_reference_row(prow, reference[bench, 'P'])
        gate = synth(start, folder / 'M/gate', mcfg['stage1'], 'Optimized mMIG',
                     f'{bench}_M_gate', original, abc)
        if gate['sha256'] != stage1_reference[bench]['sha256']:
            raise RuntimeError(f'{bench}: frozen M Stage 1 byte hash drift')
        final = synth(gate['path'], folder / 'M/phase', mcfg['stage2'], 'Optimized mMIG',
                      f'{bench}_M_phase', original, abc)
        final['runtime_s'] += gate['runtime_s']
        if topology(gate['path'])['topology_hash'] != topology(final['path'])['topology_hash']:
            raise RuntimeError(f'{bench}: M Stage 1/2 topology changed')
        mrow = flow_row(bench, seed, 'M', sha(start), sha(original), final, abc)
        verify_reference_row(mrow, reference[bench, 'M'])
        rows.extend((trow, prow, mrow))
        write_csv(GENERATED / 'three_flow_results.csv', rows)
        new_lower = lower_min(bench, gate['path'], final['path'], original, abc)
        for lr in new_lower:
            ref = lower_reference[bench, lr['stage']]
            for key in ('G', 'D', 'I_raw', 'I_nonconst', 'MAJ_count',
                        'MIN_count', 'topology_hash', 'sha256'):
                if str(lr[key]) != str(ref[key]):
                    raise RuntimeError(f'{bench}/{lr["stage"]} {key} drift')
        lower_rows.extend(new_lower)
        write_csv(GENERATED / 'min_lowering.csv', lower_rows)
        write_csv(GENERATED / 'matched_mig_stages.csv', stage_rows)
        if not args.skip_runtime:
            rr, aa = runtime_sensitivity(bench, stages[0][1], final['runtime_s'], original, abc)
            runtime_rows.append(rr)
            attempts.extend(aa)
            write_csv(GENERATED / 'runtime_sensitivity_current_machine.csv', runtime_rows)
            if attempts:
                write_csv(GENERATED / 'runtime_sensitivity_attempts.csv', attempts)
        print(f'{bench}: T={trow["G"]}/{trow["I_nonconst"]} '
              f'P={prow["G"]}/{prow["I_nonconst"]} '
              f'M={mrow["G"]}/{mrow["I_nonconst"]} CEC PASS', flush=True)
    if args.verify_only and set(args.benchmarks) == set(BENCHES):
        write_csv(GENERATED / 'three_flow_results.csv',
                  read_csv(ROOT / 'results/reference/three_flow_results.csv'))
        write_csv(GENERATED / 'min_lowering.csv',
                  read_csv(ROOT / 'results/reference/lowered_mmig_results.csv'))


if __name__ == '__main__':
    main()
