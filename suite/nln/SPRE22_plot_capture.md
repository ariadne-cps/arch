# SPRE22 plotting capture experiment

This experimental branch makes SPRE22 perform the expensive evolution and
property checks **once**, then write a compilable C++ header containing the
validated constrained-image drawing inputs of its entire `orbit.reach()`.
SPRE22 intentionally **does not plot** in the capture run.

The capture includes the parameter domains, Taylor coefficients, uniform
errors, supported sweeper policies, constraints and projection indices used
by the drawing layer. It is not a dump of bounding boxes, nor a serialization
of the original `HybridEnclosure` objects, which have no general deserializer.
It exercises the same drawer followed by batched recording and ordered replay.
It cannot reproduce bugs in the earlier construction or copying of
`HybridEnclosure` objects.

From the `arch` repository root:

```sh
git switch spre22-plot-reproducer
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build-release --target SPRE22 --parallel 8
./build-release/suite/nln/SPRE22 -c max
```

The output is `build-release/spre22_plot_data.generated.hpp`.
To capture a range rather than the whole orbit, set
`SPRE22_PLOT_FIRST` (zero-based index) and `SPRE22_PLOT_COUNT`
before the capture process (for example
`SPRE22_PLOT_FIRST=1024 SPRE22_PLOT_COUNT=256 ...`).
A complete capture may be very large; creating and compiling a header
with hundreds of thousands of Taylor models may require substantial disk
space, memory and compiler time. The header is generated into the build
directory and should not be committed. If capture is interrupted, the
`.partial` file retains the written prefix for inspection.

After capture:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build-release --target SPRE22_plot_replay --parallel 8
./build-release/suite/nln/SPRE22_plot_replay -c max
```

The second binary performs no evolution. Its drawing jobs use the
same 256-item batching, parallel `RecordingCanvas` operations and
sequential canvas replay as `ListSet<HybridEnclosure>::draw()`. It logs
the index range of each replay batch. Try `-c 0` to compare sequential
behavior. The output is `SPRE22_replay.png`.

The generator intentionally fails on unsupported function-model or
sweeper types instead of silently substituting approximate geometry.

## Compile-time smoke test

The normal CMake build also creates a small synthetic `HybridEnclosure` with
bounded and one-sided (infinite) constraints, runs the capture generator, and
compiles and links the emitted C++ header through `SPRE22_plot_replay_smoke`.
No real evolution or plotting occurs during this smoke check. This ensures
that the code generation round trip is checked by macOS/Linux/Windows CI
before another full SPRE22 run. It is not a test of the size or performance of
the full snapshot.
