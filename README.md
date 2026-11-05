# Ariadne ARCH suite

Experimental ARCH examples moved from `ariadne-cps/ariadne/examples/arch`
without changing their C++ sources. They live in `suite/`.

The build compiles seven executables: `ROBE25`, `CVDP23`, `LALO20`,
`LOVO25`, `SPRE22`, `SUTR21`, and `arch_suite`. The additional
`QUAD20.cpp` is preserved, but cannot be compiled: the original repository
does not provide its required `QUAD20.hpp`. The original CMake also omitted
this program.

## Standalone build against an installed Ariadne

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/ariadne/install
cmake --build build --parallel
```

The `suite/` folder can also be configured directly using
`cmake -S suite -B build-suite -DCMAKE_PREFIX_PATH=/path/to/install`.
Both modes use the shared `cmake/FindAriadne.cmake` and the installed
`ariadne.hpp` umbrella header.

## CI

Ubuntu, macOS and Windows build and install the pinned `ss/ard` submodule,
then compile the suite against that installation. **The suite is not
executed in CI** because these are potentially expensive experiments.
