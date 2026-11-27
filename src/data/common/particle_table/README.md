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
| `extra_table` | none | JSON file of extra particles, resolved with `path_at` (relative to `ufw-basepath`) |

Each particle of the file is added to the table, or replaces the particle with the same code (the log names each
replaced code). The file is read by `particle_table`; ROOT's global `TDatabasePDG` is not modified.

### Format

```json
{
  "source": "CODATA 2022 nuclear masses (no electrons), NIST",
  "particles": [
    { "pdg": 1000020040, "name": "alpha", "class": "Ion", "mass": 3727.3794118, "charge": 2 }
  ]
}
```

At the top level, two keys, both required: `source` (string: where the values come from) and `particles` (array).
Each entry of `particles` has five keys, all required:

| Key | Type | Meaning |
|---|---|---|
| `pdg` | integer | PDG code |
| `name` | string | name |
| `class` | string | particle class, as ROOT's (`"Ion"`, `"Lepton"`, ...) |
| `mass` | number | mass in MeV, i.e. CLHEP units: stored as written, ≥ 0 |
| `charge` | integer | charge in units of e |

The charge is an integer: the extra particles have no fractional charge (quarks and diquarks are already in ROOT's
table).

Antiparticles are explicit entries with a negative code and their own values: nothing is generated from the
particle, and an entry with a negative code does not need the opposite one. Replacing a particle does not replace
its antiparticle: list both to change both.

A missing or unknown key, a value of the wrong type, a negative mass, a non-integer charge, a code repeated in the
file, a missing file or invalid JSON stop the job with `particle_table::invalid_table`, naming the file and the
entry (index in `particles` and, when it can be read, the PDG code).

### Light nuclei

`light_nuclei.json` (installed in `<prefix>/share/sandreco/data/particle_table/`) adds the light nuclei, which are
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
  "sand::particle_table": { "extra_table": "particle_table/light_nuclei.json" }
}
```

With `ufw-basepath` set to the sandreco data directory (`/usr/local/share/sandreco/data` in the container), as in
the framework tests.

## Files

| File | Content |
|---|---|
| `particle_table.hpp/.cpp` | the framework data `sand::particle_table` |
| `light_nuclei.json` | optional table of the light nuclei |

Tests: `tests/standalone/particle_table`.
