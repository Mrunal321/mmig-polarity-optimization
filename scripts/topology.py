#!/usr/bin/env python3
"""Stable BLIF incidence signature; intentionally ignores truth tables and phase."""

import hashlib
import json
from pathlib import Path


def topology(path):
    lines = [line.strip() for line in Path(path).read_text().splitlines()]
    inputs = []
    outputs = []
    names = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith('.inputs '):
            inputs = line.split()[1:]
        elif line.startswith('.outputs '):
            outputs = line.split()[1:]
        elif line.startswith('.names '):
            words = line.split()[1:]
            cubes = []
            i += 1
            while i < len(lines) and not lines[i].startswith('.'):
                if lines[i] and not lines[i].startswith('#'):
                    cubes.append(lines[i])
                i += 1
            names.append((words[:-1], words[-1], cubes))
            continue
        i += 1
    if not inputs or not outputs:
        raise ValueError(f'missing BLIF ports: {path}')
    ids = {name: ('PI', j) for j, name in enumerate(inputs)}
    gates = []
    aliases = {}
    for fanins, output, cubes in names:
        if len(fanins) == 0:
            ids[output] = ('CONST', 0 if cubes == ['0'] or not cubes else 1)
        elif len(fanins) == 3:
            try:
                incidence = tuple(sorted((ids[name] for name in fanins), key=str))
            except KeyError as exc:
                raise ValueError(f'non-topological BLIF {path}: {exc}') from exc
            gates.append(incidence)
            ids[output] = ('G', len(gates) - 1)
        elif len(fanins) == 1:
            aliases[output] = fanins[0]
            ids[output] = ids[fanins[0]]
        else:
            raise ValueError(f'unexpected {len(fanins)}-fanin record in {path}: {output}')
    po = [ids[name] for name in outputs]
    data = {'pi_count': len(inputs), 'po_count': len(outputs),
            'gate_count': len(gates), 'gate_incidence': gates, 'po_sources': po}
    signature = hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()
    return {k: v for k, v in data.items() if k != 'gate_incidence'} | {'topology_hash': signature}
