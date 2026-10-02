# ESL artifact release audit

This file records the verification status of the public `v1.0-esl-submission` artifact. The frozen structural results are measurements of graph representations, not physical inverter or area reductions.

| Question | Finding |
|---|---|
| Public GitHub URL | https://github.com/Mrunal321/mmig-polarity-optimization (public repository verified with `gh repo view`) |
| Release/tag | `v1.0-esl-submission`; the tag is created only after the clean-clone test. |
| Clean clone tested | Yes. Cloned public `main` into a fresh temporary directory, with no development checkout dependency or `MMIG_ABC` override. Tested source commit `77f07db3bcdc0fb87728c89c45d97a3953ca11d3`; full run elapsed 749 s on 2026-10-03. |
| `./reproduce.sh` passes | Yes in the isolated release tree using a local ABC; elapsed 639 s. |
| Docker reproduction tested | Yes: `./reproduce.sh --docker` passed in 638 s on 2026-10-03. |
| Primary deterministic numbers reproduced | T 15454 G / 6749 I / 5863 LUT6; P 15205 / 6501 / 5815; M 15070 / 6021 / 5867. M versus P: −0.89% G, −7.38% I. |
| CEC checks and failures | 120 required raw ABC checks passed, zero failed; the parser's equivalent and inequivalent regression pairs also passed. |
| Paper regenerated | Yes; generated tables, figure, and `paper/main.pdf` built in Docker. GitHub artifact link is a clickable PDF annotation. |
| Paper page count | Four. |
| External dependency not pinned | Vendored source bytes and ABC Git commit are pinned. Some vendored upstream commit IDs could not be recovered; this provenance limit is documented in `THIRD_PARTY_NOTICES.md`. Docker Ubuntu base is digest-pinned and packages use a dated Ubuntu snapshot, with CA certificates bootstrapped from the base repository. |
| Remaining absolute local path | No absolute workstation path in the tracked release content; path-pattern scan reported zero. |
| Secret scan findings | No high-confidence credential, private key, GitHub token, or local-path match in staged release content. `gitleaks` was unavailable; see `artifact/PUBLIC_RELEASE_AUDIT.md`. |
| Third-party licensing issue | No identified blocker. Notices are retained and listed in `THIRD_PARTY_NOTICES.md`; the separate ABC binary is built from pinned public source. |
| Machine-dependent result | The secondary pure-MIG time-budget sensitivity is wall-clock dependent and stored separately from deterministic paper totals. |
| Exact commit used by manuscript link | The manuscript names the immutable `v1.0-esl-submission` tag. Resolve its exact commit with `git rev-parse v1.0-esl-submission^{commit}`; the literal SHA is recorded on public `main` after tag creation. |

The container and clean clone reproduced the direct MIN-lowering sequence `6021 → 7028 → 6668` nonconstant complemented edges. The clean-clone command was `env -u MMIG_ABC -u MMIG_IN_DOCKER /usr/bin/time -p ./reproduce.sh` after `git clone https://github.com/Mrunal321/mmig-polarity-optimization.git`.
