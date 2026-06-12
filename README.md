# bbb-dmx

Header-only C++ core and JSON specifications for the `bbb.dmx` ecosystem.

This repository intentionally contains only Max/openFrameworks-independent pieces:

- `include/bbb/dmx/*.hpp` — fixture profiles, patches, JSON loader, value mapping, mover tracking math, frame utilities, scene/palette/matrix-map helpers.
- `schemas/*.schema.json` — machine-readable JSON Schemas for bbb.dmx interchange files.
- `docs/*.md` — canonical format and converter specifications.

Max externals live in [`2bbb/bbb.dmx`](https://github.com/2bbb/bbb.dmx). openFrameworks integration lives in [`2bbb/ofxBbbDmx`](https://github.com/2bbb/ofxBbbDmx).

## CMake

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Consumers can add this repository as a submodule and include `include/`, or use the `bbb::dmx` CMake interface target via `add_subdirectory`.
