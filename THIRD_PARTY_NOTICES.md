# Third-party source and data

The top-level MIT `LICENSE` does not replace the notices in vendored source files. Their original headers are retained. The exact released bytes are pinned in `artifact/SHA256SUMS`; several vendor snapshots in the development tree did not retain upstream Git commit metadata, so an upstream commit is **unknown** rather than guessed.

| Component | Upstream | Version/commit information | License/notice | Delivery |
|---|---|---|---|---|
| modified mockturtle core | https://github.com/lsils/mockturtle | development snapshot at parent commit `78b701d2387629ad8f9858e9fad3b718d2ddfa34`, with additional dirty changes; exact file hashes in manifest | MIT; top-level `LICENSE` and source headers | vendored `include/`, `examples/` |
| EPFL combinational BLIFs | https://github.com/lsils/benchmarks | benchmark file hashes in `benchmarks/MANIFEST.csv`; upstream commit unavailable | MIT, `third_party/licenses/EPFL_BENCHMARKS_LICENSE` | vendored twelve BLIFs |
| ABC command-line tool | https://github.com/berkeley-abc/abc | `ee899284b854bbd37acc9ddbe1dc598c9a2cf151` | Berkeley copyright notice, `third_party/licenses/ABC_copyright.txt` | fetched and built by `scripts/build.sh`; no binary committed |
| abc-staticlib-derived SAT/ESOP source | https://github.com/lsils/abc-staticlib | vendored snapshot, upstream commit unavailable | Berkeley/ABC and retained MiniSat/Glucose notices in source | vendored `lib/abcsat`, `lib/abcesop` |
| bill | https://github.com/lsils/bill | vendored snapshot, upstream commit unavailable | MIT and retained solver notices | vendored `lib/bill` |
| kitty | https://github.com/lsils/kitty | CMake snapshot label v0.4; upstream commit unavailable | MIT source headers | vendored `lib/kitty` |
| lorina | https://github.com/lsils/lorina | CMake snapshot label v0.1; upstream commit unavailable | MIT source headers | vendored `lib/lorina` |
| percy | https://github.com/lsils/percy | vendored snapshot, upstream commit unavailable | MIT, plus BSD notice in `concurrentqueue.h` | vendored `lib/percy` |
| fmt | https://github.com/fmtlib/fmt | CMake snapshot label v6.3.0; upstream commit unavailable | BSD, `lib/fmt/LICENSE.rst` | vendored `lib/fmt` |
| nlohmann/json | https://github.com/nlohmann/json | CMake snapshot label v3.5.0; upstream commit unavailable | MIT, retained header notice | vendored `lib/json` |
| parallel-hashmap / Abseil portions | https://github.com/greg7mdp/parallel-hashmap | CMake snapshot label 2020.11; upstream commit unavailable | Apache-2.0, retained headers and `third_party/licenses/Apache-2.0` | vendored `lib/parallel_hashmap` |
| rang | https://github.com/agauniyal/rang | vendored snapshot, upstream commit unavailable | MIT upstream; exact source byte pinned | vendored `lib/rang` |
| IEEEtran LaTeX class | https://www.ctan.org/pkg/ieeetran | included v1.8b class file | LaTeX Project Public License v1.3, notice retained in `paper/IEEEtran.cls` | vendored `paper/IEEEtran.cls` |
| IEEEtran BibTeX style | https://www.ctan.org/pkg/ieeetran | included v1.14 style file | LaTeX Project Public License v1.3, notice retained in `paper/IEEEtran.bst` | vendored `paper/IEEEtran.bst` |

The diagnostic `src/phase_audit.cpp`, frozen flow configuration, and runner scripts are from this mMIG project. The local Testa-style control uses our implementation; no code from the Testa paper was copied into this artifact. There are no commercial EDA binaries or proprietary libraries in the release tree.
