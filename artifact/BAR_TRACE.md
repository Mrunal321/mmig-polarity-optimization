# bar: fixed-profile regression trace

A separate logging-only build of the hashed frozen optimizer produced a **byte-identical Stage 1 BLIF** to archived `zg2`. It did not change the profile, output, or manuscript.

| Point | G | D | Nonconstant I | Raw I | MIN | Evidence |
|---|---:|---:|---:|---:|---:|---|
| Archived start | 2511 | 14 | 703 | 1411 | 0 | archived `gate.log` |
| After minority seeding | 2511 | 14 | 486 | 1192 | 9 | logging-only `runs/bar/bar_trace/gate.log` |
| After pre-rewrite | 2511 | 14 | 486 | 1192 | 9 | logging-only `runs/bar/bar_trace/gate.log` |
| After post-rewrite/inversion passes | 2511 | 14 | 486 | 1192 | 9 | logging-only `runs/bar/bar_trace/gate.log` |
| After window resynthesis | 2511 | 14 | 486 | 1190 | 17 | logging-only `runs/bar/bar_trace/gate.log` |
| After advanced round 1 | 2481 | 17 | 786 | 1415 | 0 | logging-only `runs/bar/bar_trace/gate.log` |
| After advanced round 2 | 2464 | 17 | 667 | 1248 | 12 | logging-only `runs/bar/bar_trace/gate.log` |
| After BLIF import into Stage 2 | 2464 | 17 | 845 | 1425 | 0 | archived `phase.log`; native MIN labels removed |
| After mixed post-phase | 2464 | 17 | 672 | — | 0 | archived `phase.log` |
| After two-phase / final | 2464 | 17 | 624 | 1210 | 45 | archived `phase.log` |

Seeding reduces nonconstant edges from 703 to 486 without changing gate count. Window resynthesis has zero gate gain. Round 1 reduces gates to 2481 but raises nonconstant edges to 786; round 2 reduces both to 2464 gates and 667 edges. Stage 2 import changes the represented edge phases to 845 nonconstant edges while preserving incidence topology, then mixed and two-phase optimization reduce the count to 624. This remains **above** the Testa-style 486-edge result.

The matched pure MIG finishes at 2460 gates and 675 edges. The `bar` regression stays in the primary table. The diagnostic source and exact command are saved in `build_bar_trace.py`, `bar_trace.compile_command.json`, and `runs/bar/bar_trace/command.json`.
