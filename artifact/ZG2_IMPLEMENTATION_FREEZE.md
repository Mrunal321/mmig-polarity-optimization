# Frozen zg2 implementation

Git HEAD: `78b701d2387629ad8f9858e9fad3b718d2ddfa34`.

Dirty tree: **yes** (114 porcelain entries before this freeze document was written). The archived binary, rather than HEAD alone, identifies the executed implementation. This document and the reviewer-control files do not alter zg2.

Archived synthesis binary: `projects/mmig-esl-validation/experiments/paper_ready_comparison/build/blif2mig_paper`; SHA-256 `36d561aad283359660a66fdf16e041acb14e0abc1740a837458c93e8376bf4a2`.
ABC: `<ABC executable>`; SHA-256 `2641644f15299dcffe3a988cce95fab189ff199d91b98d86fe5f1ba5f60b1173`.

Archived results: `results/zg2/joint_discovery.csv`, 12 circuits, with per-circuit command, binary hash, output hash, runtime, and raw CEC path.

## Exact Stage 1 CLI (after input and output prefix)

```text
--mode=area --mig-flow=identity --import-lut --enable-mmig --mmig-flow=compress2rs --mmig-max-iters=3 --mmig-stage=both --mmig-cec --mmig-advanced --mmig-dont-cares --mmig-interleaved-seeding --mmig-window-resyn --mmig-window-cut=4 --mmig-window-internal=8 --mmig-window-candidates=2048 --mmig-window-matches=8 --mmig-window-roots=128 --mmig-window-min-inv-gain=1 --mmig-adv-objective=gate_inv --mmig-window-objective=gate_inv --mmig-window-max-inv-increase=0 --mmig-window-allow-output-polarity --mmig-window-rank-local-inv --mmig-adv-no-balance --mmig-advanced-rounds=2 --mmig-adv-max-inv-increase=1000 --mmig-allow-zero-gain
```

## Exact Stage 2 CLI (after Stage 1 BLIF and output prefix)

```text
--mode=area --mig-flow=identity --import-lut --enable-mmig --mmig-skip-opt --mmig-post-phase --mmig-two-phase
```

## Pass order and semantics

Relevant implicit defaults in `examples/blif2mig_2.cpp` and `mmig_optimizer.hpp`: minority seeding mode `both`, seed budget 24, one seed round; advanced resubstitution/cut/refactoring/exact enabled; advanced stagnation limit one round; don't-care threshold 10,000 gates; interleaved reseeding every second round; dual inversion propagation enabled; auto SR5 with CEC enabled; rewrite ranking and complemented-inner normalization enabled; resubstitution candidate ranking and tuned advanced policy enabled. The optional inverter-first polarity stage is disabled. Window depth preservation and gate-increase acceptance are disabled. The area-mode advanced exact-rewrite automatic size threshold is 3,500 gates when CEC is enabled. Other parameters are the defaults in the hashed source, not benchmark-specific overrides.

Stage 1 imports the archived MIG with identity MIG flow, then calls `mmig_area_optimization` (`examples/blif2mig_2.cpp`). It runs: minority seeding; algebraic rewrite; inversion propagation; inversion optimization; post algebraic rewrite; cone-polarity flip; post inversion propagation; post inversion optimization; bounded window resynthesis; two `compress2rs` advanced rounds. Each round interleaves mMIG algebraic rewrite, resubstitution, cut rewrite and refactoring, including zero-gain cut/refactoring stages and conditional exact rewrite. Advanced balancing is disabled. Inter-round minority reseeding may run at round 2. Per-pass internal CEC guards are enabled.

The window uses cut 4, maximum 8 internal nodes, 2,048 expression candidates, 8 matches/root, 128 roots, output polarity and local-inverter ranking. Window `gate_inv` compares gates, then native complemented edges, then depth, with 0 allowed inversion increase on a gate gain. The advanced-round `gate_inv` ordering is the same; a gate reduction is admissible with up to 1,000 more *native raw complemented edges*. Equal-gate rounds need non-increasing inverted edges; the objective guard can roll back a regressed round. Area mode does not enforce non-increasing depth. The algebraic-rewrite maximum is 3 iterations.

**Serialization:** Stage 1 writes `gate_mmig_opt.blif`; Stage 2 reimports that BLIF (`--import-lut`). The old driver checks equal gate counts but no topology signature. BLIF does not carry an explicit MAJ/MIN type field, so type preservation and fixed topology must be audited rather than assumed. Stage 2 skips structural mMIG optimization, then applies `esl_mixed_phase::optimize` and `esl_two_phase::optimize`, and writes `phase_mmig_opt.blif`.

This control keeps the original two-process behavior to preserve the archived result byte for byte. A new native Stage 1→2 chain would start Stage 2 from the mixed gate labels instead of the BLIF-reimported pure-MAJ graph and would define a different algorithm. The separate diagnostic `phase_audit.cpp` replays Stage 2 **in memory after the same BLIF import**, records type vectors, and directly expands native MIN for the representation control. The BLIF incidence topology audit covers the actual archived Stage 1→2 boundary.

## Relevant source SHA-256

| File | SHA-256 |
|---|---|
| `examples/blif2mig_2.cpp` | `90d66f4b8011cb5ca2c6742317a47d0c2aeeb31ec1e25849ab63cc8ad5a899a6` |
| `include/mockturtle/algorithms/mmig_optimizer.hpp` | `888a006c53bb716d85439402901bce8cfa0421c73a9111a673fae58361da8f33` |
| `include/mockturtle/algorithms/mmig_window_resynthesis.hpp` | `f66030526f3fd98a03dce7c9dd82feaded6c8542cf93b577f41bbfbd3a9a8408` |
| `include/mockturtle/algorithms/mmig_algebraic_rewriting.hpp` | `82cf3e6806a0455caa6cdd820478af065a576745fac7821da3ade41c57382b3e` |
| `include/mockturtle/algorithms/mmig_inv_propagation.hpp` | `ff2991fd495720cb2e42c19d5db9aabd36f34811f29de5a0eddeeeb9c5631c7c` |
| `include/mockturtle/algorithms/mmig_minority_seeding.hpp` | `36b5bcf738e9e5ca03eefae5f5bf1c36a8306be3eaf0634d6704d80033de7d3d` |
| `include/mockturtle/algorithms/mmig_cone_polarity_flip.hpp` | `bd2d72a8a26af6d4c75840877c31c0240c8f825ab83ab74924169f3fd07172d0` |
| `include/mockturtle/algorithms/mmig_resubstitution.hpp` | `874e694dd2d8e96598c3ecc17d7165db25d52da776c39e4262c2e4b649ad79fb` |
| `include/mockturtle/algorithms/mmig_exact_rewriting.hpp` | `e7c103e2f3a00b9e42759005c788ec7e645b6beb954a9364528131dbb6810472` |
| `include/mockturtle/networks/mig.hpp` | `0d335f095c2cd1d2be059ed3a8f29d0947ebe03ff109813d14390e24c5c98c8c` |
| `projects/mmig-esl-validation/experiments/mmig_competitive/mixed_phase.hpp` | `2a29b51439468d23fe2ce659f9a739c354eafbcd016f1c825925f0c0901e96fd` |
| `projects/mmig-esl-validation/experiments/mmig_competitive/two_phase.hpp` | `5d436ef8ccb263d41c9b441ba2838c2d408c1b7d5407f6720f268f8e4a1f9a24` |
| `projects/mmig-esl-validation/experiments/mmig_joint_improvement/run_joint_flow.py` | `ed6c5bfcb094ce12f184cbb643baa86ec3d13ddf41a0a3bccbbe189dd7be4b5c` |
| `projects/mmig-esl-validation/experiments/esl_mmig_window/profile.py` | `7b3db212c08ea00471f2660d3e2cbe788729879d730348c6b29653f5e2470f52` |

## Archived output hashes

| Benchmark | seed | Stage 1 BLIF SHA-256 | final BLIF SHA-256 |
|---|---:|---|---|
| adder | 1 | `493b2463a306dec34ceb4d8d7576ae317a317ae1c7fe7d3e00680f6caa77649d` | `493b2463a306dec34ceb4d8d7576ae317a317ae1c7fe7d3e00680f6caa77649d` |
| arbiter | 1 | `12ba7596a36ed406459eec154821e76ad239d489654660badd4e86ad566f47be` | `b75ecce5f6143ac6611facace76c70eecb40414f1900c478b0e71d113ef29116` |
| bar | 2 | `90cdd512c95030e9065692e51c36d20c3c938ea41527665a2e98579772abecd0` | `7438f7b2ca1b610a674bd01d3c718e4a23de90d0e68cf89da8a871a0514e31a3` |
| cavlc | 1 | `fbb4e557e8531eee7c275d878d4514968e9edc7e4cbe0c3f031e9056c7787881` | `784506a54473014aecfbb46250f7d16642ec62095aabc0601b73207e6a83a346` |
| ctrl | 3 | `96fb84f9348e6cf4efea3458f580954a6705e924b12a3712791fd8d81ff317db` | `4963ef7d9c0b175e337b7a382ad366d9738e2e464f65d8e9334d6e49d5cb6815` |
| dec | 1 | `7ab640d5547f0e896a52211825b49c9f7190d02065164a6d8b540844f9ea23b8` | `ee386ef7ea22faab146262f21b4028a5f82fceef7d34ad88fdea60d2b98a7a27` |
| i2c | 3 | `f9fbbb8d5070a0bed74ba1af365ee4a364c02d6d7928dfa78b769d2035c30bdf` | `76ad93869f11056791eb52d635a60fafaeb822d6c647a7637549340b6de10dc2` |
| int2float | 3 | `cb0fdf4d60ca415cc62236814b84f83e226081b645286fdddab68f9a5595c37a` | `34ecdf28903f5058022374bb6ce13f2a8be5cbc095edcbe6d0b575eea575b02f` |
| max | 1 | `8511da37bbdeef206bdd1ad12eec673dc98309387ed9146d7be2f9a8968a576e` | `d81b273a14761d0e39a4b114d76a79946ad5f96137ee6481dfda3abf14287d3f` |
| priority | 2 | `739b7a16f2799ca8b3e7ee9f55ecc0f2820ad5e100ed447193f0987f41449831` | `dad813f3bfaef6c87ed6c260523c4ac9013dc5fad9106c746f58e749c65b4c89` |
| router | 2 | `8526aa01c3bf4040209bf367006f4e25982bd3ebab897f4f508e1c1076288dc2` | `a487c555d53e74c9b5a5e87abea901e56dd047e3531367be5ffb6e591fa1c8cd` |
| voter | 1 | `ee77c7ea98e1f6a2f18662a103f9877bb2e7538a3754a4441ac07ede28558dc8` | `2d0bececaa0440ea5660f9e91d8e3da8016a9ee81338f82c5a36f0e7079ca32f` |

The experiment was profile-tuned on these benchmarks; the selected input MIG per circuit came from earlier mMIG-score selection. No manuscript file is changed by this control study.
