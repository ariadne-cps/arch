# Ariadne ARCH

[![Build](https://github.com/ariadne-cps/arch/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/ariadne-cps/arch/actions/workflows/ci.yml?query=branch%3Amain)

C++ benchmark suite for the Ariadne framework, organized under `suite/nln/`.
It contains the following executables:

- `ROBE25`
- `CVDP23`
- `LALO20`
- `LOVO25`
- `SPRE22`
- `arch_suite`, which runs the five benchmarks above in sequence.

## Build

The repository includes Ariadne as a Git submodule at `ss/ard`. Ariadne and
the benchmark executables are compiled in the **same CMake build tree**,
without installing Ariadne or finding an external installation.

```sh
git submodule update --init -- ss/ard
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build --parallel
```

## Continuous integration

GitHub Actions checks compilation on Ubuntu, macOS, and Windows, using
`sccache` when available. The benchmarks are **not executed** in CI.
