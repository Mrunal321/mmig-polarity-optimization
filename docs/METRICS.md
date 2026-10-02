# Metrics and interpretation

`G` = reachable logic nodes, `MAJ_count + MIN_count`. `D` = graph logic depth. `I_raw` = all complemented graph connections in the printed graph, including constant-source cases. `I_nonconst` = complemented graph connections with nonconstant sources, including primary output connections. `total_edges` = three fanin connections per logic node plus primary output connections. These are **structural graph metrics**.

`LUT6` and `LUT_depth` are measured by ABC after `strash; if -K 6 -a` for every final T/P/M network. They are not standard-cell area or routed delay.

A MIN node carries output inversion implicitly. Native M edge count therefore cannot be interpreted as physical inverter count. The direct MIN-lowering control expands each native MIN to MAJ/INV without changing gate-incidence topology, then applies the same pure-MIG phase optimization. Its edge counts provide a representation-sensitive comparison. The main result is a native graph advantage, not a physical cost advantage.
