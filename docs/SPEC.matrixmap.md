# SPEC.matrixmap.md — Jitter matrix to multi-universe DMX mapping

## 1. Purpose

`bbb.dmx.matrixmap` samples color data from a `jit.matrix` and writes those values into fixture parameters using the existing fixture patch/profile layer.

It is a mapper, not an output object. It emits canonical multi-universe frames:

```text
universe <id> <512 byte values>
```

Network output remains outside this repository. Feed the result to `bbb.dmx.merge`, `bbb.dmx.fade`, `bbb.dmx.safety`, then a sender such as `bbb.artnet`.

## 2. Object

```max
[bbb.dmx.matrixmap @patch patches/rgb-grid.example.json @map maps/rgb-grid.example.json]
```

Input:

- `jit_matrix <name>` — read one Jitter matrix by registered name and patch sampled values into fixture parameters.
- `readpatch <path>` — load fixture patch JSON.
- `readmap <path>` — load matrix map JSON.
- `reload` — reload current patch and map files.
- `bang` — output current DMX buffers without re-sampling.
- `dump` — output load status.

Attributes:

- `@patch <path>` — fixture patch JSON.
- `@map <path>` — matrix map JSON.
- `@universe <id>` — selected universe when `@universe_mode selected`.
- `@universe_mode all|selected|changed` — output all patched universes, one selected universe, or only universes whose full frame changed since the last output.
- `@plane_order rgba|argb|bgra|gray` — plane layout of incoming `char` or `float32` matrices.
- `@gamma <0.01..8.0>` — gamma curve applied to sampled color components.
- `@brightness <0.0..8.0>` — post-gamma multiplier.
- `@autobang 0|1` — output immediately after `jit_matrix`.
- `@invert_x 0|1`, `@invert_y 0|1` — mirror normalized sample coordinates.

Input value policy:

- `char` matrices are interpreted as byte values and normalized with `byte / 255.0`.
- `float32` matrices are interpreted directly as normalized values and clamped to `0.0..1.0`.
- The normalized sample is written through the fixture parameter type: `u8` => `0..255`, `u16` => `0..65535`, `u24` => `0..16777215`.
- `@gamma` and `@brightness` operate in normalized space before the value is expanded to the fixture bit depth.

Limitations:

- Outputs are full 512-channel universe frames, even in `changed` mode. `changed` means changed universes, not changed-channel deltas.
- Higher-resolution output only helps if the fixture profile uses `u16` or `u24`; an 8-bit `char` source cannot invent precision beyond its original 256 levels.

## 3. Matrix map JSON schema

Top-level file:

```json
{
  "schema": "bbb.dmx.matrixmap.v1",
  "grid": {
    "fixture_pattern": "pixel_%03d",
    "start_index": 1,
    "cols": 2,
    "rows": 2,
    "mode": "average",
    "params": {
      "red": "r",
      "green": "g",
      "blue": "b"
    }
  }
}
```

The mapper supports three top-level mapping forms:

- `fixtures`: explicit per-fixture sample regions.
- `grid`: one regular grid.
- `grids`: multiple regular grids.

All forms can coexist. They are appended in file order: `fixtures`, then `grid`, then `grids`.

### 3.1 Explicit fixture mapping

```json
{
  "fixtures": [
    {
      "id": "pixel_001",
      "sample": { "x": 0.25, "y": 0.25, "mode": "average", "w": 0.5, "h": 0.5 },
      "params": { "red": "r", "green": "g", "blue": "b" }
    }
  ]
}
```

Fields:

| Field | Type | Meaning |
|---|---|---|
| `id` | string | Fixture id in the loaded patch. |
| `sample.x`, `sample.y` | number | Normalized matrix coordinates, `0..1`. |
| `sample.mode` | `point` or `average` | Single-pixel lookup or rectangular average. |
| `sample.w`, `sample.h` | number | Normalized average region size. Only used by `average`. |
| `params` | object | Fixture parameter name to color source mapping. |

### 3.2 Grid mapping

Grid mapping is for LED/pixel arrays where fixture ids follow a predictable sequence.

```json
{
  "grid": {
    "fixture_pattern": "pixel_%03d",
    "start_index": 1,
    "cols": 16,
    "rows": 8,
    "mode": "average",
    "params": { "red": "r", "green": "g", "blue": "b" }
  }
}
```

Rules:

- `fixture_pattern` is a `printf`-style integer format string. Example: `pixel_%03d` expands to `pixel_001`.
- Fixture order is row-major: left-to-right, top-to-bottom.
- `cols` and `rows` must be positive.
- The generated cell center becomes `sample.x/y`; the cell size becomes `sample.w/h`.

### 3.3 Color sources

Valid source strings:

| Source | Meaning |
|---|---|
| `r` / `red` | sampled red component, normalized `0..1` |
| `g` / `green` | sampled green component, normalized `0..1` |
| `b` / `blue` | sampled blue component, normalized `0..1` |
| `a` / `alpha` | sampled alpha component, or `1.0` if absent |
| `luma` | Rec. 709 luma from RGB |
| `maxrgb` | maximum RGB component |
| `constant:N` | fixed value. `0.0..1.0` is normalized; values greater than `1.0` are treated as legacy byte constants and divided by `255`. |

Numeric JSON values inside `params` are accepted with the same constant rule: `0.5` means half-scale, `255` means full-scale for backward compatibility.

## 4. Example package files

- `fixtures/generic.rgb.3ch.json` — generic 8-bit RGB profile.
- `fixtures/generic.rgb.16bit.json` — generic 16-bit-per-color RGB profile.
- `fixtures/generic.rgb.24bit.json` — generic 24-bit-per-color RGB profile.
- `patches/rgb-grid.example.json` — four RGB pixels split across universes 1 and 2.
- `maps/rgb-grid.example.json` — 2x2 matrix grid mapped to those four fixtures.

This small example is deliberately multi-universe. If your design only proves a 512-channel single-universe path, it will fail the moment the patch grows.
