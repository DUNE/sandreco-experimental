# sandreco-experimental — Agent Guidelines

## Project Overview
`sandreco` is a reconstruction framework for DUNE/ND/SAND, based on the [ufw](https://baltig.infn.it/dune/ufw) framework. It produces analysis-oriented CAF-like `.root` files. The project is C++17, built and deployed inside containerized environments from `baltig.infn.it:4567/dune/sand-ci/sandreco-ci`.
The reconstruction flow includes steps such as digitization, clustering, track finding, etc... Each step is represented by a module.

## Code Layout
```
include/          — Public headers + managed data (ufw managed types)
  common/         — Shared data structures (track, digi, hit, sand, array, scalar, etc.)
  grain/          — GRAIN subdetector types (grain, image, photons, voxels, point_cloud, digi)
  ecal/           — ECAL subdetector types (slice, photo_electron, digit)
  tracker/        — Tracker types (digi, cluster_container)
  caf/            — CAF wrapper
src/
  processes/      — Algorithm modules (ufw::process subclasses)
  filters/        — Filter modules
  streamers/      — I/O streamers (rarely written from scratch)
  data/           — Complex data (geometry, truth info, etc.)
tests/
  standalone/     — Boost-based unit tests
  framework/      — Framework-level integration tests (.json configs)
  data/           — Small test data files
tools/cmake/      — Custom CMake modules
docs/             — Doxygen docs, writing_new_modules.md
```

## Module Development
Each algorithm, data type, or streamer is its own module (shared library). Use the provided CMake helpers:
- `sandreco_add_process(name)` — for process/algorithm modules
- `sandreco_add_complex_data(name)` — for complex data modules (exports headers)
- One CMake per module, one library per CMake

### Naming conventions
- Namespace: `sand::` for general things, otherwise `sand::<subdetector>::` (e.g. `sand::grain::`, `sand::tracker::`)
- Module/library name: fully qualified class name with `::` replaced by `_` (e.g. `sand_grain_detector_response_fast`)
- Folder structure mirrors the namespace hierarchy under `src/processes/<subdetector>/`, `src/filters/<subdetector>/`, etc.

### Module structure
Each module needs:
1. A CMakeLists.txt using `sandreco_add_process` or `sandreco_add_complex_data`
2. A `.hpp` header with `UFW_REGISTER_PROCESS` macro (for processes)
3. A `.cpp` source with the implementation and factory registration

See `docs/writing_new_modules.md` for a detailed guide.

## Code Style
- **Indent**: 2 spaces, no tabs
- **Line width**: 120 characters
- **Braces**: K&R-style (Allman-like for inheritance lists)
- **Short functions/lambdas**: allowed on a single line; if/for/while blocks: never
- **Include sorting**: case-sensitive, ordered by category (`<` system → `<third-party` → `"internal"`)
- `clang-format` config is in `.clang-format`. Apply with `clang-format -i <file>` or via the CMake `ClangFormat` module.

## Dependencies
Critical external libraries: `ROOT`, `ufw`, `spdlog`, `pybind11`, `duneanaobj`, `Boost` (tests). Complex data libraries like `sand_geoinfo` are internal dependencies listed in each module's `target_link_libraries`.

## CI/CD
- **GitLab CI** (`.gitlab-ci.yml`): builds with Debug + docs, installs, runs ctest
- **GitHub Actions** (`.github/workflows/build.yml`): same build/test flow
- Both use the `baltig.infn.it:4567/dune/sand-ci/sandreco-ci` container image

## Building and Testing Inside Container

All building and testing must be performed inside the `sandreco-ci` podman container, which is running.
 - The container provides the full development environment with all dependencies (ROOT, ufw, spdlog, etc.) pre-installed.
 - Dependencies are not installed outside the container and building will not be possible outside of it.
 - The container mounts the project folder `/home/nicko/Development/sand_mnt/sandreco-experimental` under a different name: `/home/sand/sandreco-experimental/`.
 - All building and testing command must be run inside the container by using `podman exec -w /home/sand/sandreco-experimental sandreco-ci bash -c "command"`
 - Beware: tests can be slow to execute and produce copious output text. Do not run tests unless authorized.

### Configuring command
 - Default command `podman exec -w /home/sand/sandreco-experimental/build/ sandreco-ci bash -c "cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug -DBUILD_DOCUMENTATION=OFF -DCMAKE_INSTALL_PREFIX=/usr/local/"`


### Build and install command
 `podman exec -w /home/sand/sandreco-experimental/build/ sandreco-ci bash -c "cmake --build . -j8 && cmake --install .`

### Run all tests command
 `podman exec -w /home/sand/sandreco-experimental/build/ sandreco-ci bash -c "ctest --output-on-failure`

### Run one test command
 `podman exec -w /home/sand/sandreco-experimental/build/ sandreco-ci bash -c "ctest -R <test name>`
 test name will be onw of the tests from `tests/standalone` or `tests/framework`

### Available tests
- **Standalone tests**: `tests/standalone/` — Boost.UnitTestFramework, header-only test executables. Include `tools/cmake/standalone_test.cmake` and use `add_test_with_libs()`.
- **Framework tests**: `tests/framework/` — sample `.json` configs that exercise the full pipeline. Keep test data under ~100KB.
- New tests should be added here too

