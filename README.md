# sandreco-experimental

This is a new area for experimenting with porting sandreco to ufw and updating all public data structures

## Repository organization

The basic organization of folders is dictated by the nature of a framework-based application:
 - public interfaces, i.e. mostly data strucutres used outside of sandreco, are declared in include/
 - private interfaces, that are not exposed outside of the framework, are in src/common/
 - utility code that is used in several modules but is not a module, also goes in src/common/
 - modules that wrap data structures, both public and private, are in individual subfolders of src/products/
 - top level algorithms are in individual subfolders of src/processes/
 