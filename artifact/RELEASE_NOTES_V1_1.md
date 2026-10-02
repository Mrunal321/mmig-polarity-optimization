# v1.1-esl-revision

Presentation revision of the four-page manuscript and reproducibility artifact. Table I now shows the per-circuit complemented-edge count for the existing local Testa-style pure-MIG control T alongside P and M. The caption explicitly distinguishes T from Testa et al.'s original program. The text reports that M improves on T on ten circuits, ties on `adder`, and loses on `bar` (624 versus 486).

The frozen starting networks, synthesis flows, original benchmark functions, reference CSV values, ABC CEC procedure, and LUT6 mapper are unchanged from `v1.0-esl-submission`. Primary M-versus-P results remain −0.89% MAJ+MIN nodes and −7.38% nonconstant complemented graph connections. All 120 required raw ABC CEC checks pass. Run `./reproduce.sh` for the full four-page paper and experiment.

The complemented-edge count is a graph metric, not physical inverter count. The secondary pure-MIG time-budget result varies by machine.
