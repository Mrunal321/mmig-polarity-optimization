# Polarity-Aware Logic Optimization with Mixed Majority-Minority Inverter Graphs

This repository is the reproducibility artifact for the four-page IEEE Embedded Systems Letters manuscript by Mrunal Shende. It packages the frozen twelve-circuit EPFL comparison of a local Testa-style pure-MIG polarity control (T), a matched pure-MIG structural and phase control (P), and the native mMIG flow (M). All three arms use the same per-circuit frozen starting MIG and the same ABC LUT6 mapper.

[Read the manuscript PDF](paper/main.pdf).

## Main result

Compared with P, M has **7.38% fewer nonconstant complemented graph connections** and **0.89% fewer MAJ+MIN logic nodes** in aggregate. These connections are a graph metric, **not physical inverter cells**. Directly lowering MIN to MAJ/INV and reoptimizing polarity removes M's edge-count advantage relative to P. No physical area, energy, inverter, or equal-runtime claim follows.

## Quick reproduction

```sh
git clone https://github.com/Mrunal321/mmig-polarity-optimization.git
cd mmig-polarity-optimization
./reproduce.sh
```

The command builds pinned ABC from source, builds the frozen synthesis implementation, checks input hashes, reruns T/P/M, performs raw ABC CEC, maps all outputs, runs direct MIN lowering and the machine-dependent runtime sensitivity, regenerates CSVs/tables/figure, verifies the manuscript's deterministic numbers, and compiles `paper/main.pdf`. The first run needs network access to fetch the pinned ABC commit and system dependencies. Subsequent synthesis does not call online services. For a container environment use `./reproduce.sh --docker`; for the deterministic paper data only use `./reproduce.sh --skip-runtime`; for a quicker check of committed reference networks use `./reproduce.sh --verify-only`. `./reproduce.sh --smoke` runs the one-circuit CI check.

The measured clean-clone reproduction took 749 s on the tested machine; the full Docker run took 638 s with cached image and dependencies. These are observed runtimes, not guarantees for another computer. Details are in `RELEASE_AUDIT.md`.

## Contents

- `benchmarks/`: twelve original BLIFs, their fixed starting MIGs, hashes and selected seeds.
- `examples/`, `include/`, `lib/`, `src/`: frozen modified mockturtle source and the native MIN-lowering diagnostic.
- `configs/`: exact T, P, M and mapper settings. M retains its Stage 1 BLIF write and Stage 2 BLIF reimport.
- `results/reference/`: archived submission networks and numeric CSVs, kept separate from new runs.
- `results/generated/`: new CSVs, networks, CEC transcripts, maps and timing sensitivity after reproduction.
- `paper/`: manuscript source, generated tables and figure, PDF, and structured claim manifest.
- `docs/`: methods, metric definitions and reproduction details.

`G` is the number of MAJ plus MIN nodes. `I_nonconst` counts complemented connections sourced by a nonconstant signal, including primary output connections. `D` is graph depth. `LUT6` and LUT depth are obtained with `strash; if -K 6 -a` in ABC. Every reported final network is checked against the original EPFL BLIF with raw ABC CEC; the parser rejects explicit inequivalence and counterexamples.

This is a comparison on a development set with selected starting MIGs. T is a **local Testa-style control**, not an execution of the original Testa implementation. The secondary pure-MIG search is wall-clock dependent; its archived numbers are kept for the paper, while a rerun is saved separately. See [METHODS.md](docs/METHODS.md) and [METRICS.md](docs/METRICS.md) before interpreting the results.

The top-level `LICENSE` covers the modified mockturtle code and artifact scripts. Vendored dependencies retain their own notices; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No DOI is claimed for the manuscript.
