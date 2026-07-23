# gauss_smearing

Takes the "perfect" `common_reco_branch_wrapper` produced by `fast_reco` (`in_common`) and
perturbs it with a Gaussian smearing model, producing a degraded `out_common`. The `nd`
branch (tracks/showers) is untouched by this process — smear that separately if needed.

## Data flow

```mermaid
flowchart TB
    in["in_common
(SRCommonRecoBranch)"] --> ixn["ixn.sandreco[i]
SRInteraction"]
    ixn --> smi["smear_interaction()"]
    smi -->|"vtx (x/y/z_resolution)"| outixn["out_common.ixn.sandreco[i]"]
    smi -->|"Enu.* (energy_resolution)"| outixn

    ixn --> part["ixn.sandreco[i].part.sandreco[j]
SRRecoParticle"]
    part --> smp["smear_particle()"]
    smp -->|"E (energy_resolution)"| outpart["out_common...part.sandreco[j]"]
    smp -->|"p (momentum_resolution)"| outpart
    smp -->|"start/end (x/y/z_resolution)"| outpart

    outixn --> out["out_common
(SRCommonRecoBranch)"]
    outpart --> out
```

## Configuration

| Parameter Name        | Type   | Unit          | Required/Default | Description                                                     |
|------------------------|--------|---------------|-------------------|-------------------------------------------------------------------|
| `energy_resolution`   | double | dimensionless | Default: 0.0      | `sigma_E = energy_resolution * sqrt(E)` (E in GeV).               |
| `momentum_resolution` | double | dimensionless | Default: 0.0      | `sigma_p = momentum_resolution * \|p\|`.                          |
| `x_resolution`        | double | cm            | Default: 0.0      | Gaussian sigma for the x coordinate of every smeared position.   |
| `y_resolution`        | double | cm            | Default: 0.0      | Gaussian sigma for the y coordinate.                             |
| `z_resolution`        | double | cm            | Default: 0.0      | Gaussian sigma for the z coordinate.                             |

## Notes

- All resolutions default to `0.0`: an unconfigured `gauss_smearing` is a no-op copy of
  `in_common` to `out_common`.
- `smear_interaction()`/`smear_particle()` are free functions, independently testable and
  reused as-is by `run()` — no hidden state beyond the resolutions read in `configure()`.
- Randomness comes from `ufw::context::current()->engine()`, the per-context RNG already
  seeded by the framework; `gauss_smearing` does not own or seed its own engine.
- `smear_interaction()` mirrors `fast_reco`'s choice of collapsing every `Enu` estimator
  (`calo`/`lep_calo`/`mu_range`/...) to the same value — here, the same smeared energy —
  rather than smearing each independently.
