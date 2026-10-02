"""Portable, deliberately small helpers for the frozen ESL experiment."""

import csv
import hashlib
import json
import re
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / 'results/generated'
BENCHES = ('adder', 'arbiter', 'bar', 'cavlc', 'ctrl', 'dec', 'i2c',
           'int2float', 'max', 'priority', 'router', 'voter')
MAPPING = 'strash; if -K 6 -a'
POSITIVE = re.compile(r'^Networks are equivalent(?: after structural hashing)?\.', re.MULTILINE)
NEGATIVE = re.compile(r'NOT EQUIVALENT|counterexample|not equivalent', re.IGNORECASE)
STATS = re.compile(r'\bnd\s*=\s*(\d+).*?\blev\s*=\s*(\d+)')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def relative(path):
    return str(Path(path).resolve().relative_to(ROOT))


def read_csv(path):
    with Path(path).open(newline='') as stream:
        return list(csv.DictReader(stream))


def write_csv(path, rows):
    if not rows:
        raise ValueError(f'empty CSV: {path}')
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def execute(command, log, timeout=900):
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    try:
        process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                 timeout=timeout)
        output = process.stdout + process.stderr
        code = process.returncode
    except subprocess.TimeoutExpired as exc:
        chunks = (exc.stdout or '', exc.stderr or '')
        output = ''.join(x.decode(errors='replace') if isinstance(x, bytes) else x
                         for x in chunks)
        output += f'\nTIMEOUT after {timeout}s\n'
        code = 124
    seconds = time.monotonic() - start
    log.write_text('COMMAND: ' + json.dumps(command) + '\nRETURN_CODE: '
                   + str(code) + '\n' + output)
    return code, seconds, output


def parse_metrics(output, section):
    marker = f'=== {section} ==='
    at = output.rfind(marker)
    if at < 0:
        raise ValueError(f'missing {section} metrics')
    text = output[at + len(marker):].split('===', 1)[0]
    patterns = (r'PIs=(\d+)\s+POs=(\d+)\s+Gates=(\d+)\s+Depth=(\d+)',
                r'GateTypes:\s+MAJ=(\d+)\s+MIN=(\d+)',
                r'InvEdges\(total\)=(\d+)', r'InvEdges\(nonconst\)=(\d+)')
    matches = [re.search(pattern, text) for pattern in patterns]
    if not all(matches):
        raise ValueError(f'incomplete {section} metrics')
    pi, po, gates, depth = map(int, matches[0].groups())
    maj, min_count = map(int, matches[1].groups())
    if gates != maj + min_count:
        raise ValueError('gate count does not equal MAJ + MIN')
    return {'pis': pi, 'pos': po, 'G': gates, 'D': depth,
            'I_raw': int(matches[2].group(1)),
            'I_nonconst': int(matches[3].group(1)),
            'MAJ_count': maj, 'MIN_count': min_count}


def _normalize_ports(source, dest):
    text = Path(source).read_text(errors='replace')
    logical = []
    current = ''
    for raw in text.splitlines():
        line = raw.rstrip()
        if line.strip().endswith('\\'):
            current += line.strip()[:-1].strip() + ' '
        else:
            current += line
            logical.append(current.strip())
            current = ''
    if current:
        logical.append(current.strip())
    def ports(directive):
        for line in logical:
            if line.startswith(directive + ' '):
                return line.split()[1:]
        return []
    inputs, outputs = ports('.inputs'), ports('.outputs')
    if not inputs or not outputs:
        raise ValueError(f'missing BLIF ports in {source}')
    mapping = {name: f'x{i}' for i, name in enumerate(inputs)}
    mapping.update({name: f'y{i}' for i, name in enumerate(outputs)})
    pattern = re.compile(r'(?<![^\s])(' + '|'.join(re.escape(n) for n in
                         sorted(mapping, key=len, reverse=True)) + r')(?![^\s])')
    Path(dest).write_text(pattern.sub(lambda m: mapping[m.group(1)], text))
    return len(inputs), len(outputs)


def cec(abc, original, candidate, transcript):
    """Use raw ABC CEC on port-normalized, strashed BLIFs; reject negative text."""
    transcript = Path(transcript)
    transcript.parent.mkdir(parents=True, exist_ok=True)
    work = transcript.parent / (transcript.stem + '.work')
    work.mkdir(exist_ok=True)
    a, b = work / 'a.norm.blif', work / 'b.norm.blif'
    aa, bb = work / 'a.strash.blif', work / 'b.strash.blif'
    entries = []
    try:
        if _normalize_ports(original, a) != _normalize_ports(candidate, b):
            raise ValueError('input/output port counts differ')
        for source, target in ((a, aa), (b, bb)):
            cmd = [str(abc), '-c', f'read_blif {relative(source)}; strash; write_blif {relative(target)}']
            rc, _, out = execute(cmd, work / (target.stem + '.log'), 120)
            entries.append(f'COMMAND: {json.dumps(cmd)}\nRETURN_CODE: {rc}\n{out}')
            if rc or not target.exists():
                raise ValueError('ABC strash failed')
        cmd = [str(abc), '-c', f'cec {relative(aa)} {relative(bb)}']
        rc, _, out = execute(cmd, work / 'cec.log', 120)
        entries.append(f'COMMAND: {json.dumps(cmd)}\nRETURN_CODE: {rc}\n{out}')
        good = rc == 0 and POSITIVE.search(out) and not NEGATIVE.search(out)
        if not good:
            raise ValueError('ABC did not explicitly report equivalence')
    except Exception as exc:
        entries.append(f'CEC_ERROR: {exc}')
        transcript.write_text('\n'.join(entries) + '\n')
        raise RuntimeError(f'CEC failed: {transcript}') from exc
    transcript.write_text('\n'.join(entries) + '\n')
    return transcript


def map_lut6(abc, network, output, log):
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(abc), '-c', f'read_blif {relative(network)}; {MAPPING}; print_stats; '
           f'write_blif {relative(output)}']
    rc, seconds, text = execute(cmd, log, 900)
    match = STATS.search(re.sub(r'\x1b\[[0-9;]*m', '', text))
    if rc or not output.exists() or not match:
        raise RuntimeError(f'ABC LUT6 mapping failed: {log}')
    return int(match.group(1)), int(match.group(2)), seconds
