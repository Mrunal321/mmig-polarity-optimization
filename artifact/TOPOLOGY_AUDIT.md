# Topology and native MIN audit

`results/reference/TOPOLOGY_AUDIT.csv` records the Stage 1 BLIF to Stage 2 BLIF gate-incidence hashes. `results/reference/NATIVE_TOPOLOGY_AUDIT.csv` records the imported, native, raw-lowered and phase-optimized lowering signatures and gate-type hashes. The rerun checks the same gate-incidence topology and compares the diagnostic native Stage 2 BLIF byte-for-byte with the M result before accepting the ablation. The source BLIF cannot preserve explicit MIN labels; the diagnostic records the labels in separate `.types` files.

The fixed aggregate edge counts are native M 6,021; direct raw MAJ-only lowering 7,028; and lowering plus pure-MIG phase 6,668. The matched pure-MIG P count is 6,501. See `docs/METRICS.md` for why these are structural connections rather than physical inverters.
