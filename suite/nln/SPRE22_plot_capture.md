# SPRE22 isolated binary plot replay

The branch `spre22-plot-reproducer` keeps SPRE22's evolution and safety
checks unchanged. After the expensive orbit computation, it exports the exact
binary64 coefficients, Taylor uniform errors, sweeper policies, parameter
domains, constrained-model bounds (including ±infinity), and projection indices
consumed by the drawing layer. It does **not** draw during capture.

The output is `build-release/spre22_plot_data.bin`, written via
`build-release/spre22_plot_data.bin.partial` and atomically published only
on successful completion. The binary is a documented versioned SPRE22-only
format (not a serialization of full `HybridEnclosure` objects). A previously
generated `.generated.hpp.partial` file is not reusable; it can be deleted.

From the repository root, after switching to the branch and updating the
checkout, configure/build as usual:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build-release --target SPRE22_plot_roundtrip_smoke
cmake --build build-release --target SPRE22 SPRE22_plot_replay
./build-release/suite/nln/SPRE22 -c max
./build-release/suite/nln/SPRE22_plot_replay -c max
```

`SPRE22_plot_roundtrip_smoke` creates a small synthetic reach with finite,
unilateral infinite and bounded constraints, writes a binary snapshot and
reads it back with field/count validation. The CI builds this target and
executes its capture/read operation on Ubuntu, macOS and Windows without
running SPRE22's evolution.

Capture a subset using the zero-based environment variables
`SPRE22_PLOT_FIRST` and `SPRE22_PLOT_COUNT` (e.g. `SPRE22_PLOT_COUNT=256`).
The replay reads bounded 256-enclosure batches and draws with separate
`RecordingCanvas` objects in parallel, then replays into one rendering
canvas sequentially. Each batch prints its original index range.

The generated binary format has little-endian integer counts and binary64
values, a magic string and version, per-record payload lengths and original
indices, and a strict readback check. NaNs and non-finite coefficients and
domains are rejected; infinities are permitted only for constraint bounds.
Unknown sweeper types fail explicitly rather than silently changing geometry.
The binary format is not designed to load untrusted input and cannot reproduce
issues occurring in `HybridEnclosure` copies before the drawer.
