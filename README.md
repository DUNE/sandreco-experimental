# sandreco-experimental

This is the current development branch of the sandreco software. The previous, non modular, version has been renamed sandreco-legacy.

This software is based on [ufw](https://baltig.infn.it/dune/ufw), and it is meant to be developed and deployed inside a [containerized environment](https://baltig.infn.it/dune/sand-ci).

A sandreco job is based on a `.json` configuration that specifies what modules to run and their configuration.

The typical goal of a "production" job is to produce a CAF-like file, an analysis-oriented `.root` file whose content matches the SAND portion of [the full CAF format](https://github.com/DUNE/duneanaobj).
You can find a description of the data fields filled at present [here](src/processes/fake_reco/fields.md).

## Developer information

This is a quick overview of module development.

### Modular approach

Each algorithm in ufw must be wrapped in a process, while data structures can be complex or managed (see the ufw wiki for details).
Additionally, streamers exist to provide custom I/O interfaces for managed types.

Each algorithm, complex data and streamer needs to be its own module, which in concrete terms is a shared library (libmodulename.so or similar).
Managed data on the other hand is not a module (as it is header only, by definition it does not have code that needs to be compiled).

A module needs its own cmake which produces the shared library that will be loaded by ufwrun.
For cleanliness, one cmake shall produce only one module.

A module should have its own folder, placed according to these indications in the most suitable subfolder:

 - Managed data, which will also be used for public interface classes, i.e. data structures used also outside of sandreco,
   should be under `include/.../`
 - Complex data can go in `src/data/.../`
 - Algorithms go in `src/processes/.../`
 - Filters go in `src/filters/.../`
 - Streamers go in `src/streamers/.../`  You will probably never write one of these.

The ... in the path indicated above may be either your subdetector folder, or a subfolder, if applicable.

The main class of your module (and realistically also the other ones) should go in a namespace as well.
Super general things can go in sand::, otherwise use your subdetector, lowercase `sand::grain::`, `sand::ecal::`, `sand::tracker::` and so on...
The namespace becomes part of your module name, and must be used in the configuration files.

Refer to [this](docs/writing_new_modules.md) page for more details.

### Automatic testing

You can place a sample `.json` that runs your module (and any ancillary modules used in your test) in tests/framework.
It will be picked up by the testing system and run as part of the continuous integration.
Only do this if you can supply a meaningful input test data that is reasonably small (under a few 100kb), as large binary files shall not be stored in baltig. Some large data files are already available in `/usr/local/share/sandreco/data/`; if you have a justified need for a larger file, we can bake it into the CI image.
