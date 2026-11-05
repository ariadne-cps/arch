# Ariadne ARCH suite

Experimental ARCH examples moved from `ariadne-cps/ariadne/examples/arch`
with the source files under `suite/nln/`. Their entry points now explicitly
use the Drawing command-line interface instead of the removed
`ariadne_main.hpp` wrapper.

The build compiles seven executables: `ROBE25`, `CVDP23`, `LALO20`,
`LOVO25`, `SPRE22`, `SUTR21`, and `arch_suite`. The additional
`QUAD20.cpp` is preserved, but cannot be compiled: the original repository
does not provide its required `QUAD20.hpp`. The original CMake also omitted
this program.

## Build Ariadne and the suite together

The default build uses the pinned `ss/ard` submodule directly, without
installing Ariadne or searching for a previously installed library:

```sh
git submodule update --init -- ss/ard
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build --parallel
```

Each executable links the `ariadne` target produced in the same CMake
build tree.

## CI

Ubuntu, macOS and Windows configure and compile Ariadne and the NLN suite
in the same build tree, using sccache when available. **The suite is not
executed in CI** because these are potentially expensive experiments.
