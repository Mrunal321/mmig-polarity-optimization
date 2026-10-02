# Profile selection history

The first fixed profile, `esl_mmig_window_v1`, used the existing mMIG area optimizer with two `compress2rs` rounds, bounded window resynthesis, and a gate/inverter objective that allowed **zero** additional native complemented edges on gate gains. Its selected 12-benchmark window logs showed no net gate gain. Intermediate gate reductions could be rolled back when complemented-edge count rose; balancing also erased a gate reduction observed on `ctrl`.

The later `zg2` profile was chosen **after examining results on these same 12 benchmarks**. It disabled advanced balancing, allowed zero-gain advanced cut/refactoring moves, retained two advanced rounds, and raised the gate-gain native-edge allowance from 0 to 1,000. It then ran mixed-graph phase and two-phase MAJ/MIN optimization as a second stage. A one-round, +500-edge `v1`-successor (`GATE_ARGS`) was also explored. The `zg2` profile is therefore development-set tuned; its 12-circuit results are not a held-out generalization test.

The selected starting MIG per circuit was previously chosen by the minimum `6G+2I_raw` result from a three-seed mMIG search. This selection is shared by the local Testa-style and matched-MIG controls, but it is also informed by mMIG performance. There is no benchmark-specific `zg2` fallback; in particular, the `bar` regression remains in the main result.

Neither this control nor its reports modify the ESL manuscript.
