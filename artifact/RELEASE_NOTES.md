# v1.0-esl-submission

Frozen reproducibility artifact for “Polarity-Aware Logic Optimization with Mixed Majority-Minority Inverter Graphs.” Across the twelve fixed EPFL starting MIGs, the native mMIG flow M has 0.89% fewer MAJ+MIN nodes and 7.38% fewer nonconstant complemented graph connections than the matched pure-MIG control P. Raw ABC CEC checks cover twelve starting MIGs, twelve M Stage 1 outputs, 36 T/P/M finals, and 60 ablation outputs. Run `./reproduce.sh` to regenerate the fixed comparison, ablation, tables, figure and four-page manuscript.

The complemented-edge metric is not a physical inverter count. The secondary time-budget sensitivity varies by machine and is not required to match the archived numeric result.
