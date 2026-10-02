# Frozen submission reference

`three_flow_results.csv`, `lowered_mmig_results.csv`, and the associated BLIF networks preserve the final paper's deterministic metrics and network SHA-256 values. Path-valued columns were rewritten to repository-relative locations; no numerical fields were changed. `runtime_sensitivity_paper.csv` is the archived secondary result and must not be overwritten by a current-machine run.

`m_stage1/` and `m_stage1_hashes.csv` preserve the intermediate frozen zg2 BLIF outputs so both M stages can be checked byte-for-byte.

`manuscript.pdf` is an immutable release-reference copy of the linked four-page submission manuscript. `paper/main.pdf` is the build output and may change at the byte level across LaTeX installations while retaining the verified page count and content.

`cec/` contains 120 fresh release-staging checks of these frozen files against the original EPFL functions. ABC stdout/stderr and return codes are preserved; only the workstation executable path in command metadata was anonymized. `./reproduce.sh` generates new raw local transcripts under `results/generated/cec/`.
