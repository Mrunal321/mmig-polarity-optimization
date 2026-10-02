# v1.1 ESL presentation revision

The `v1.0-esl-submission` tag remains unchanged. This revision adds the per-circuit $T:I$ column to manuscript Table I and states the observed T-versus-M exception on `bar`. T is the existing local Testa-style phase control; it is not the original Testa executable or a reproduction of that paper's numeric table. Published Testa benchmarks use different starting MIGs.

No synthesis pass, starting MIG, benchmark, CEC command, mapper command, flow selection, or reference CSV result was changed. The table is generated from the same CEC-verified three-flow CSV. $M$ has fewer nonconstant complemented graph edges than $T$ on ten circuits, ties on `adder`, and has more on `bar` (624 versus 486). Aggregate T/P/M counts remain 6,749/6,501/6,021.

The paper footnote and citation metadata identify `v1.1-esl-revision`. The previous release remains available as a separately pinned version.

Validation on 2026-10-03: the full `./reproduce.sh` run completed in 796 s. The 120 required raw ABC CEC checks passed with zero failures; the positive/negative CEC parser regression passed. T/P/M totals, MIN-lowering totals, LUT6 mapping, figure generation, and the four-page PDF matched the frozen numerical claims. The final PDF and reference bytes are pinned in `artifact/SHA256SUMS`.
