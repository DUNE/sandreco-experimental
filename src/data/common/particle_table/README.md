# particle_table

Particle properties by PDG code, as framework data built once per job: name, mass, charge and particle class.
It is the only place in sandreco that reads ROOT's `TDatabasePDG`; processes use the table instead.

## Usage

In a process:

```cpp
#include <particle_table/particle_table.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>

auto const& table = instance<sand::particle_table>(); // by reference: framework data cannot be copied

if (table.contains(pdg)) {
  double const m_mev = table.mass(pdg);               // CLHEP units: MeV = 1
  double const m_gev = table.mass(pdg) / CLHEP::GeV;  // for code working in GeV
  double const q     = table.charge(pdg);             // units of e
  bool const lepton  = table.particle_class(pdg) == "Lepton";
}
```

| Function | Returns |
|---|---|
| `contains(pdg)` | `true` if the code is in the table |
| `at(pdg)` | `particle_table::particle` with `name`, `mass`, `charge`, `particle_class` |
| `mass(pdg)` | mass in CLHEP units (MeV) |
| `charge(pdg)` | charge in units of e (ROOT stores \|e\|/3: the table divides by 3) |
| `particle_class(pdg)` | ROOT's class: `"Lepton"`, `"Meson"`, `"Baryon"`, `"GaugeBoson"`, ... |

`at()`, `mass()`, `charge()` and `particle_class()` throw `particle_table::unknown_pdg`, naming the code, for a code
that is not in the table. Code that needs a fallback for unknown codes checks `contains()` first.

The table is filled in the constructor and never modified afterwards, so it is read without locks.

In the job configuration, in `globals` (ufw requires an entry for every global complex data a job uses, even with an
empty configuration; without it the job stops with `key 'sand::particle_table' not found`):

```json
"globals": {
  "sand::particle_table": {}
}
```

In CMake: `target_link_libraries(<target> PRIVATE sand_particle_table)`.

Users: `truth_filler` (is the neutrino's daughter a lepton), `gauss_smearing` and `gluckstern_smearing` (mass, for
`E`), `sand::fast::tracker::reco` and `sand::fast::grain::reco` (mass for `E`, sign of the charge),
`sand::test::fast_reco_test`.

## Content

At construction the table copies every particle of ROOT's `TDatabasePDG`, i.e. the default
`$ROOTSYS/etc/pdg_table.txt` (523 particles with ROOT 6.36, antiparticles included; no nuclei). The values are
exactly ROOT's: the mass is ROOT's value in GeV times `CLHEP::GeV`, the charge ROOT's value divided by 3. ROOT's table
stores 7 significant digits (the muon is 105.6584 MeV, the PDG value is 105.6583755 MeV).

## Extra particles: `extra_table`

| Key | Default | Meaning |
|---|---|---|
| `extra_table` | none | file in the TDatabasePDG text format, resolved with `path_at` (relative to `ufw-basepath`) |

Each particle of the file is added to the table, or replaces the particle with the same code. ROOT's global
`TDatabasePDG` is not modified: the file is parsed by `particle_table` itself, because `TDatabasePDG::ReadPDGTable`
would change the database seen by every other user of ROOT in the job, and cannot replace a code that already
exists (`AddParticle` refuses it with a warning).

### Format

The format of `$ROOTSYS/etc/pdg_table.txt`. Lines starting with `#` and empty lines are skipped. A particle line has
15 fields:

```
#    i   NAME.............  KF AP   CLASS      Q        MASS     WIDTH  2*I+1 I3 2*S+1 FLVR TrkCod N(dec)
    4 alpha         1000020040  0 100 Ion         6 3.7273794118  0.000000e+00 -100 -1    1 -1   -1   0
```

index, name, PDG code, antiparticle flag, class number (`100`, not named in ROOT's header comment), class, charge
in units of |e|/3, mass in GeV, width in GeV, 2I+1, I3, 2S+1, flavour, tracking code, number of decay channels.
`particle_table` uses the name, code, class, charge and mass; the other fields must be present but are not used. A
particle line with N(dec) > 0 is followed by N(dec) decay lines, which are skipped.

An antiparticle line has a negative code and no properties of its own: same mass and class as the particle with the
opposite code (as it is in the table at that line), opposite charge.

```
   22 mu+                 -13      21     0
```

Replacing a particle does not replace its antiparticle: list both to change both. A malformed line stops the job
with `particle_table::invalid_table`, naming the file and the line.

### Light nuclei

`light_nuclei.txt` (installed in `<prefix>/share/sandreco/data/particle_table/`) adds the light nuclei, which are
not in ROOT's table:

| PDG code | Name | Charge | Mass [MeV] |
|---|---|---|---|
| 1000010020 | deuteron | +1 | 1875.61294500 |
| 1000010030 | triton | +1 | 2808.92113668 |
| 1000020030 | He3 | +2 | 2808.39161112 |
| 1000020040 | alpha | +2 | 3727.3794118 |

Class `Ion`. Masses: nuclear masses (no electrons, as Geant4 uses for these codes), CODATA 2022 recommended values
(NIST, deuteron, triton, helion and alpha particle mass energy equivalents in MeV). The table is **not** loaded by
default; a job loads it with:

```json
"globals": {
  "sand::particle_table": { "extra_table": "particle_table/light_nuclei.txt" }
}
```

With `ufw-basepath` set to the sandreco data directory (`/usr/local/share/sandreco/data` in the container), as in
the framework tests.

## Files

| File | Content |
|---|---|
| `particle_table.hpp/.cpp` | the framework data `sand::particle_table` |
| `light_nuclei.txt` | optional table of the light nuclei |

Tests: `tests/standalone/particle_table`.
