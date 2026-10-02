# Experiment limits

The twelve EPFL inputs and starting MIGs were selected during development, before the frozen reviewer control. The artifact does not claim a fresh held-out benchmark evaluation. M's frozen profile was tuned during development on this set. T is a local Testa-style pass, not Testa et al.'s published implementation. P is the primary matched pure-MIG control and uses the same frozen starts. The independent runtime sensitivity is secondary because wall-clock budgets vary with CPU and compiler environment.

ABC LUT6 mapping is the common downstream proxy. No standard-cell, AQFP, QCA, or physical inverter conclusions are made here. The direct MIN-lowering control distinguishes a native encoding benefit from a MAJ/INV-only graph benefit.
