# Matched pure-MIG control: pass audit

`mig_matched_zg2` uses the archived `blif2mig_paper` binary and its pure-MIG passes. It is a **closest available generic-opportunity control**, not an instruction-for-instruction mirror. The same selected starting MIG, two `compress2rs` rounds, zero-gain policy, four-input cut size, area mode, final pure-MIG phase pass, ABC CEC, and LUT mapper are fixed across all 12 circuits. Generic and mMIG-specific implementations differ internally; the mismatches below must remain visible in the interpretation.

| zg2 operation | Classification | Pure-MIG counterpart | Included? | Explanation / mismatch |
|---|---|---|---|---|
| Minority seeding | mMIG-specific | none | No | Creates/selects MIN nodes. |
| Algebraic rewriting, pre/post | generic Boolean operation, mixed implementation | `mig_algebraic_depth_rewriting` in standard pre-pass and each `mighty_area` | Yes | MIG pass has different iteration controls; mMIG `max_iterations=3` has no exact MIG analogue. |
| Inversion propagation | mixed-representation operation | pure-MIG phase optimization at end | Partly | Pure MIG can move complemented edges but cannot change gate kind. |
| Inversion optimization | generic phase operation | pure-MIG phase optimization at end | Yes | Different placement in sequence. |
| Cone polarity flip | mMIG-specific | none | No | Uses MAJ/MIN polarity alternatives. |
| Bounded window resynthesis | mixed implementation of generic local optimization | standard MIG cut rewriting and `compress2rs` cut rewriting | Partly | Four-input cuts matched; no 2,048-expression/128-root limit in existing generic pass. MIG gets an additional standard pre-pass to avoid a structurally weaker control. |
| Rewriting | generic | MIG `mighty_area` and cut rewriting | Yes | Same `compress2rs` stage pattern; implementations differ. |
| Resubstitution | generic | MIG resubstitution at 6/8/10/12 inputs | Yes | Insert counts follow same depth cap pattern; divisor and window policies differ internally. |
| Cut rewriting | generic | MIG cut rewriting | Yes | Cut size 4, limit 10 where configurable. |
| Refactoring | generic | MIG refactoring | Yes | Library and local cost rules differ. |
| Zero-gain rewriting | generic | MIG zero-gain cut rewriting | Yes | Enabled in `compress2rs`. |
| Zero-gain refactoring | generic | MIG zero-gain refactoring | Yes | Enabled in `compress2rs`. |
| Don't-care use | generic | MIG resubstitution `--resub-dc` | Partly | mMIG also enables don't-cares in refactoring/exact passes below a size threshold; generic CLI does not expose identical support. |
| Balancing | generic | generic balancing exists | No | Disabled in frozen zg2, so also disabled here. |
| Advanced rounds | generic portfolio | two MIG `compress2rs` rounds | Yes | Pure-MIG round-level acceptance uses gate-first, **nonconstant**-edge allowance of 1,000, as requested. zg2 uses raw native edges; the generic inner pass guard only prevents gate increases. |
| Conditional exact rewriting | generic | `--mig-enable-exact` below the same 3,500-gate automatic eligibility threshold | Yes | Different exact libraries/candidate policies. |
| Post-phase optimization | mixed phase operation | pure-MIG fixed-topology phase optimizer | Yes | One final pure phase pass; mixed one/two-phase search has strictly larger representation space. |
| Input/output phase reassignment | mMIG-specific | pure-MIG gate phase reassignment | Partly | Independent input/output phases can toggle MAJ/MIN; unavailable in pure MIG. |
| Internal CEC guard | verification policy | raw ABC CEC after each pure structural stage | Partly | mMIG guards individual passes internally. The pure flow checks stage outputs, including every accepted round, against the original source. |

## Predeclared pure-MIG profile

1. Import the **same archived MIG** used by T and M.
2. Run the existing pure-MIG area `standard` flow once for generic preconditioning (cut rewriting, resubstitution, algebraic rewriting, post resubstitution). This is the available counterpart to mMIG pre/post and window work.
3. Run the existing pure-MIG `compress2rs` portfolio for at most two rounds, with a gate-first, nonconstant-edge-second objective and at most +1,000 *nonconstant* complemented edges when gates fall. Stop after a nonimproving round. Enable zero-gain stages and conditional exact rewriting. No balancing.
4. Run the existing pure-MIG fixed-topology phase optimizer.
5. CEC each stage against the original EPFL function and map the final network with `strash; if -K 6 -a`.

The pure MIG cannot match the mMIG window's expression/root caps or internal CEC policy through the frozen binary. If M beats P, these differences limit attribution; if P matches or beats M despite these differences, the claimed mMIG-specific advantage is unsupported.
