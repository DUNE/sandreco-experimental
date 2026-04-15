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


