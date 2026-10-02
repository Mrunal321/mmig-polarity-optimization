# Frozen EPFL inputs

`MANIFEST.csv` lists twelve original combinational BLIFs, selected starting MIGs, SHA-256 hashes and seed identifiers. The original functions are from the public [EPFL benchmark suite](https://github.com/lsils/benchmarks). The exact local source revision is pinned by the file hashes; its upstream commit was not retained. Starting MIGs were selected earlier in the project and are deliberately frozen rather than rerun with a potentially different search trajectory. Every starting MIG is checked by raw ABC CEC against its original BLIF during reproduction.
