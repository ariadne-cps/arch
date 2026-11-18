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
The original version-1 writer erroneously emitted an additional time component
when t was already part of the state variables. The replay reader accepts
those existing files (mapping size = projection dimension + 1), issues one
warning, and retains the original x/y projection without re-running evolution.
New snapshots capture the labelled mapping exactly as the original drawer
does. The round-trip smoke test checks both variants, including a synthetic
legacy record.

The binary format is not designed to load untrusted input and cannot reproduce
issues occurring in `HybridEnclosure` copies before the drawer.

## Isolate a stalled drawing batch without recapturing

The trace line `SPRE22 replay: drawing [11520,11776)` means the worker
computation has not completed; it does **not** identify which enclosure is slow.
To replay just that range from the existing multi-gigabyte snapshot, without
deserializing earlier records:

```sh
SPRE22_REPLAY_FIRST=11520 SPRE22_REPLAY_COUNT=256 SPRE22_REPLAY_TRACE=1 ./build-release/suite/nln/SPRE22_plot_replay -c max
```

Trace entries `SPRE22 draw BEGIN <original-index>` and `SPRE22 draw END
<original-index> (<seconds> s)` are emitted under a mutex. A BEGIN without
its matching END identifies a task still running, but by itself does not
distinguish an expensive computation from a deadlock. For precise isolation
replay serially one enclosure per batch:

```sh
SPRE22_REPLAY_FIRST=11520 SPRE22_REPLAY_COUNT=256 SPRE22_REPLAY_BATCH_SIZE=1 SPRE22_REPLAY_TRACE=1 ./build-release/suite/nln/SPRE22_plot_replay -c 0
```

These are replay-only environment variables; capture selection still uses
`SPRE22_PLOT_FIRST` and `SPRE22_PLOT_COUNT`. The default batch size and
behaviour remain unchanged when no replay variables are provided.
The replay reader skips uninterested records using their length prefixes,
without loading their Taylor models, and the original binary file is never
modified.
