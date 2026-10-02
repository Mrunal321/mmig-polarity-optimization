# Reproducibility

Run `./reproduce.sh` from the repository root. `--docker` builds the pinned Ubuntu base container and then runs the same command inside it. `--skip-runtime` skips only the machine-dependent secondary pure-MIG search. `--verify-only` verifies committed reference networks by SHA-256, CEC and common LUT6 mapping, then regenerates the manuscript. `--smoke` checks one circuit.

The `artifact/SHA256SUMS` manifest pins the bytes of vendored source, original and starting BLIFs, configs and reference data. `scripts/check_hashes.sh` verifies it. The ABC external source is fetched at commit `ee899284b854bbd37acc9ddbe1dc598c9a2cf151`. Set `MMIG_ABC` only for a local test with an existing ABC executable; a clean reproduction leaves it unset. The build command and compiler flags are in `scripts/build.sh`.

Source package versions are represented by the committed file hashes because the development checkout did not retain all upstream dependency commit identifiers. This is an exact source snapshot, although some upstream provenance is less precise than a submodule commit. The Docker base image is pinned by digest, and experiment dependencies use the Ubuntu repository snapshot of 2026-10-01. A CA-certificate package is bootstrapped from the base image's normal Ubuntu repository to access that HTTPS snapshot. Actual tool versions are recorded in `environment/tool_versions.txt` and `RELEASE_AUDIT.md`.

The manuscript tables use regenerated deterministic structural and LUT data plus **archived paper-machine runtime values**, because wall times cannot be expected to match on another machine. Current runtimes are in the generated CSV. All deterministic aggregate claims are checked against `paper/paper_claims.json` and cause a failure on drift. Network and per-benchmark metric hashes are also checked against the reference results.

The first run requires network for ABC and, with Docker, the base image and apt packages. No experiment stage requires a cloud service.
