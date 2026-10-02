#!/usr/bin/env python3
"""Regression for the historical 'equivalent' substring CEC false positive."""

from common import GENERATED, cec
from run_experiment import abc_path

folder = GENERATED / 'tests/cec'
folder.mkdir(parents=True, exist_ok=True)
same = folder / 'identity.blif'
opposite = folder / 'opposite.blif'
same.write_text('.model one\n.inputs x\n.outputs y\n.names x y\n1 1\n.end\n')
opposite.write_text('.model one\n.inputs x\n.outputs y\n.names x y\n0 1\n.end\n')
cec(abc_path(), same, same, folder / 'equivalent.cec.txt')
try:
    cec(abc_path(), same, opposite, folder / 'inequivalent.cec.txt')
except RuntimeError:
    text = (folder / 'inequivalent.cec.txt').read_text()
    if 'NOT EQUIVALENT' not in text.upper():
        raise RuntimeError('negative CEC did not explicitly report inequivalence')
else:
    raise RuntimeError('inequivalent circuits were accepted')
print('CEC positive/negative regression: PASS')
