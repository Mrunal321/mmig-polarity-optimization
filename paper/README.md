# Manuscript generation

`main.tex` is the four-page ESL submission source. The full `./reproduce.sh` command regenerates the numerical LaTeX tables from deterministic `results/generated` CSVs, regenerates the vector/raster figure, validates `paper_claims.json`, and compiles `main.pdf`. The summary table intentionally retains the archived submission-machine runtime values; current-machine times are kept in the generated CSV rather than inserted into the frozen manuscript.

The title in this frozen manuscript uses “Mixed Majority-Minority Inverter Graphs.” The project brief used the shorter phrase “Minority-Majority Inverter Graphs”; the artifact keeps the actual submitted manuscript title rather than silently changing it.
