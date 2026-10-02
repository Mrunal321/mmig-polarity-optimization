# Public release content audit

The release tree was assembled from an allowlist: the twelve original and frozen starting BLIFs, frozen source dependencies, exact T/P/M reference networks, direct MIN-lowering networks, numeric reference CSVs, the current four-page manuscript, and the runner/docs. The development repository's unrelated experiments, QCA/AQFP attempts, old drafts, private directories, and build products were not copied.

Before publication, scan all tracked release files for credentials and local paths using the commands below, review each hit, and record the outcome in `RELEASE_AUDIT.md`:

```sh
git grep -nEi 'password|passwd|token|api_key|secret|private_key|BEGIN RSA|BEGIN OPENSSH|github_pat|ghp_|sk-|/home/[^/]+/|synopsys'
git ls-files | grep -Ei '\.(o|so|a|exe|bin)$|(^|/)(\.env|id_rsa|id_ed25519)$'
```

Generic words in technical source code are not credentials. The manuscript author email is the public correspondence address in the submitted paper. No original CEC transcript with development-machine absolute paths was copied; the reproduction runner generates new raw ABC transcripts with release-relative input/output paths.

Third-party notices are in `THIRD_PARTY_NOTICES.md`. The separate ABC executable is fetched from a pinned public commit and built locally. No compiler binary is committed.

Staged-file scan on 2026-10-03: no tracked absolute workstation paths, high-confidence credential strings, SSH/private-key blocks, `.env` files, executables, object files, or static/shared libraries. The sole `synopsys` hit was the pattern in the audit command above. The only `secret` hits were this audit text and a generic comment in `IEEEtran.cls`. `gitleaks` was not installed; the explicit staged-content pattern scans were performed and reviewed. The paper's published correspondence email is intentional.

A filename scan of the broader development tree found `repro/dac19_legacy/pins.env`, containing repository URL/commit setting names. It was deliberately excluded as unrelated to the frozen ESL experiment. Broad scans found no high-confidence token or private-key file candidates among the source areas used for this release.
