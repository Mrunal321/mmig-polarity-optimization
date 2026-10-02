# Methods

The twelve inputs and selected seeds are fixed in `benchmarks/MANIFEST.csv`. The starting MIGs are frozen file bytes; the artifact verifies their hashes and function against the original BLIF rather than rerunning the earlier selection search. This keeps the paper experiment fixed and avoids post hoc input changes.

**T** imports each starting MIG and applies the local `--fixed-mig-inv-opt` polarity pass. This is a local Testa-style control; it is not claimed as a reproduction of Testa et al.'s implementation.

**P** runs the documented standard-MIG prepass, at most two `compress2rs` rounds with the frozen cut/resubstitution settings, a one-round stagnation stop, and the same pure-MIG phase pass. Rounds are admitted with the frozen gate/inversion criterion. Exact rewriting is enabled only at or below 3,500 gates. See `configs/matched_mig.json` and `scripts/run_experiment.py`.

**M** runs frozen zg2 Stage 1 using `configs/zg2_frozen.json`, writes a BLIF, reimports it in a second process, skips further structural optimization, then applies mixed-phase and two-phase optimization. This explicit file boundary is part of the reported experiment. BLIF does not encode native MIN labels; Stage 2 starts from reimported MAJ logic and creates its own final MIN labels. `src/phase_audit.cpp` replays Stage 2 in memory from that same BLIF and exports native, directly lowered, and lowered-plus-pure-phase controls. Gate-incidence topology is checked across the stage boundary and ablation.

For each final output, port positions are normalized, both BLIFs are strashed, and ABC runs `cec` against the **original** benchmark function. A pass needs zero ABC return status and the explicit positive equivalence sentence, with no negative-equivalence/counterexample text. Every arm is mapped using the same ABC command `strash; if -K 6 -a`. The CEC and mapper commands, logs and outputs are saved under `results/generated/`.

The time-budget sensitivity extends P with up to twenty admissible MIG rounds until it reaches the current-machine M runtime. It is secondary and machine dependent. The paper's archived time numbers remain in `results/reference/`; a rerun does not overwrite them or change the primary tables.
