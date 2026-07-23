# caf_streamer

Reads/writes CAF (Common Analysis Format) ROOT files: a single `caf::StandardRecord` per
event, stored in the `rec` branch of a configurable TTree, optionally keyed by a
`context_id` branch so it can be composed with producers/consumers that don't use the
entry index as their `ufw::context_id` (e.g. `fast_reco`/`gauss_smearing`/
`gluckstern_smearing` writing `truth`/`common`/`nd` branches in the same run). Every
variable attached to the streamer (`truth_branch_wrapper`, `common_reco_branch_wrapper`,
`nd_reco_branch_wrapper`) reads/writes a different sub-object of that *same* shared
`caf::StandardRecord` (`mc`/`common`/`nd`).

## Data flow

```mermaid
flowchart TB
    cfg["configure(cfg, op)"] -->|"uri, tree"| file["m_file (TFile)
m_tree (TTree)"]

    prep["prepare(id, type)"] -->|"type must be truth_branch_wrapper /
common_reco_branch_wrapper / nd_reco_branch_wrapper"| ok["accepted"]

    att["attach(data, id)"] -->|"first call only"| branches["rec branch (caf::StandardRecord)
context_id branch (optional, read-only)"]
    branches --> ptr["m_caf_ptr
(shared by all attached variables)"]

    read["read(context_id id)"] -->|"has context_id: linear search
m_context_id == id"| entry["m_tree->GetEntry(n)"]
    read -->|"no context_id: id used
directly as entry index"| entry
    entry -->|"fills"| ptr
    ptr -->|"mc / common / nd"| vars["attached wrappers
(via ufw::data)"]

    write["write(context_id id)"] -->|"for each attached wrapper"| ptr
    vars -->|"mc / common / nd"| ptr
    write -->|"m_context_id = id"| ptr
    write -->|"Fill()"| entry

    dtor["~caf_streamer()"] -->|"wo/rw: Write() + Close()"| file
    dtor -->|"delete m_caf_ptr"| ptr
```

---
