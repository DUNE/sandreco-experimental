# Writing a new module

This guides assumes you already familiar with ufw documentation on writing processes and data.
Sandreco uses ufw managed data for most physics data objects. These are found typically under `include/`.

Some critical pieces of data, and some wrappers around data sources that are considered complex data are found under `src/data/` instead.

The typical algorithm would consist of a process module, to be placed under `src/processes`, which requires some managed data as input and produces some other managed data as output. The process would then also internally reference complex data, such as accessing the geometry or truth information. If the structures you find in `include/` are already suitable as your inputs and outputs, you only need to write an algorithm as a subclass of `ufw::process`.

## A simple process

Let us examine a simple process as an example: `src/processes/grain/detector_response_fast`.
First of all the module is contained in its own directory. This directory needs to be added to the `CMakeLists.txt` of the parent directory.
Since this algorithm is specific for the GRAIN subdetector, its parent is the `grain` directory.

### CMake

The module is a single cpp class, with its header and source files, plus a `CMakeLists.txt`.
Examining the latter, we find:

```cmake
sandreco_add_process(sand_grain_detector_response_fast)
```
This wraps the cmake add_library call, setting up some internal dependencies. The name will become the plugin filename, so it must be unique.
For consistency use the fully qualified name of the c++ class, replacing `::` with `_`.

```cmake
target_sources(sand_grain_detector_response_fast PRIVATE detector_response_fast.cpp)
```
This lists the source files that make up the module. It is not strictly necessary to list headers, but you can add those too.

By default, all processes already know about all the managed data found under `include/`, and they also know all the common complex data in `src/data/common`. If you need additional includes, you may add for example this line:

```cmake
target_include_directories(sand_grain_detector_response_fast PRIVATE "${CMAKE_SOURCE_DIR}/src/data/grain")
```

The final portion is the trickiest: You need to list the libraries that your module depends on. For external libraries, refer to their documentation for correct naming. If it is not ROOT, which is already found in the main cmake, you will also need to `find_package` it or equivalent. Bear in mind that complex data are libraries that need to be listed here (use the name their cmake target name). This module only uses geometry information, and therefore links `sand_geoinfo`.

```cmake
target_link_libraries(sand_grain_detector_response_fast PRIVATE sand_geoinfo)
```

In general, if your module is only an algorithm, you can list all sources, includes and links as `PRIVATE`.

### Header

Refer to the ufw documentation for subclassing `ufw::process`.
Try to keep the header clean of unnecessary dependencies. Use forward declarations if you must.

Remember to UFW_REGISTER_PROCESS here, while the factory goes in the .cpp

If you see this error when building

```
#error "This file (version.h) must be included before any ufw headers"
```

you need to make sure that sand headers are included before ufw headers in all cases.

### Source

Go wild implementing your algorithm. Use `get<T>()` for managed inputs, `set<T>()` for managed outputs and `instance<T>()` for complex data.
Remember to write a doxygen comment for the class which includes a table of the parameters used in `configure()`.

## Streamers

Streamers are tricky. Use the existing ones unless you really need a different file type and cannot convert. Fully featured streamers need to handle I/O of everything under `include/`. If you are an experienced developer, check `tree_streamer` for guidance. A streamer that handles only a handful of types could be made as well, such as `png_streamer`, that only writes 2D images.

## Data

Complex data are also somewhat tricky. Some important differences with processes, other than intended use/purpose, is in their build.
Complex data are used internally by other modules, so some of their headers (the primary class one, at minimum) need to be exported.
The `sandreco_add_complex_data` functions handles that for you (it also adds the headers as sources).
Some external tools (e.g. the 3D viewer) also use these headers, so they are publicly exported and installed if `EXPORT_PRIVATE_INTERFACES` is true.
Another requirement for internal use is to export the link dependencies, which should therefore be PUBLIC.

## Shared-object stores and typed indices

When several records in the same context refer to the same shared object (e.g. a vertex shared by many tracks), store the object once in a per-context store and address it through a typed index (key) instead of duplicating it. The store and the records holding keys into it are streamed as branches of the same `TTree` entry for the same context, so a key can never dangle on read-back.

The primitive is provided by ufw (`<ufw/store.hpp>`); this section covers the sandreco-side conventions for using it.
A complete, minimal example (base element type, store, keyed record, and producer/resolver processes) is in
`include/common/shared.h` and the demo processes in `tests/framework/016_modules` and `017_modules`,
exercised by the framework tests `016_store_write.json` / `017_store_read.json`.

### Declaring a store

1. Define the element type `T` as a plain class in a header under `include/`
   (`sand::shared_object` is the base for typical shared objects).
2. Declare the store with `SAND_SHARED_COLLECTION` (declared in `include/common/data.h`,
   next to `SAND_DATA_COLLECTION`, which it mirrors):

   ```cpp
   SAND_SHARED_COLLECTION(sand, vertex, vertices)
   ```

   This produces, in namespace `sand`:
   - `sand::vertices` — the collection type (`ufw::data::managed_collection<sand::vertex>`),
     registered as managed data under this name;
   - `sand::vertices_store()` — accessor for the current context's store;
   - `sand::vertices_index` — the typed key for that store, registered as unmanaged data.

   `NAME` also becomes the public id of the store variable (the name used in a
   process's `reqs`/`prods` and as the streamer branch name). The public id space is
   flat — namespaces do not qualify it — so `NAME` must be **unique across all stores**;
   choose it to disambiguate stores that would otherwise share a name in different
   namespaces (e.g. `grain_hits` rather than `hits`, see below).

   The element type is registered as unmanaged data by the macro as well, so it is
   ROOT-streamable. Declare one store per object class you share; each gets its own name and
   its own index type, and the same element type may be used by several stores.
3. No cmake work is needed: the ROOT streamer picks `SAND_SHARED_COLLECTION` lines under
   `include/` up automatically when it generates its linkdef (element, collection and index
   classes are linked).

### Holding keys in records

A managed record holds a key as a plain member:

```cpp
struct track {
  // ...
  sand::vertices_index vertex; // key into the vertices store
};
```

The key is the runtime index type, with the exact layout of a single `std::size_t` in all
environments (including Cling), so records stream as usual; the ROOT streamer needs no special
handling for keys.

### Filling the store and minting keys

The process that *produces* the store variable (it lists the store in its `prods`) creates the
empty per-context store. Any process may also list a store in its `reqs` to add objects and mint
keys into its records:

```cpp
auto& store = sand::vertices_store();
auto idx    = store.size();
store.push_back(vertex);
auto key    = sand::vertices_index{idx};
```

### Resolving keys

The resolver process reads the record(s) and dereferences the keys:

```cpp
const auto& track = get<sand::track>("in_track");
if (track.vertex) {                 // explicit operator bool: valid against the current store
  const auto& vertex = *track.vertex;
}
```

The store is attached lazily, the first time a key is dereferenced (or otherwise used). The
read-ordering guarantee — the framework calls the streamer's `read` on every attach to a read
streamer, and `tree_streamer::read` re-populates the already-read entry on a repeat call —
ensures the store is populated regardless of the order in which the process touches its record
and its store.

### Multiple stores: `reco::hit` and `reco::track`

Objects of several related stores may need to be consumed together, polymorphically, through a
common base. The convention in this codebase is one store per concrete type (here: one per
subdetector hit type — `SAND_SHARED_COLLECTION(sand::grain, hit, grain_hits)` and
equivalents for `sand::tracker` and `sand::ecal`; the element types currently stub `sand::reco::hit`, whose accessors are
virtual precisely so this works). A record that spans the stores holds **one vector of typed
keys per store** and exposes a facade that iterates over them as if they were a single container
of the base type. `sand::reco::track` is the reference example:

```cpp
track.push(sand::grain::grain_hits_index{0});    // one overload of push per store
trk.for_each_hit([](const reco::hit& h) {  // resolves each key against its store,
  h.width();                               // calls through the virtual base
});
```

The key vectors stream as plain integers, exactly like any other key. The order in which
`for_each_hit` visits the stores is **unspecified**; a defined ordering (e.g. along the track)
is a property of a view over the keys, not of the raw iteration.

This pattern is exercised by the framework tests `018_track_hits_write.json` /
`019_track_hits_read.json` with the demo processes in `tests/framework/018_modules`
and `tests/framework/019_modules`.

### Per-context / same-entry guarantee

Both the store (one branch) and the records holding keys into it (one branch each) live in the
same `TTree` row for the same context. On read-back the row is populated together, so `bool(key)`
is true and `*key` resolves to the element the key pointed at when the file was written. Keys are
per-context: they are only meaningful while the minting context's store is the current one.


