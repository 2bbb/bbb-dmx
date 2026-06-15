# SPEC.fixturemap.md — fixture data and 512-channel universe mapping

## 1. Goal

Add a common fixture-mapping layer for the `bbb.dmx.*` suite.

The layer takes:

- one or more DMX universe buffers (`512` channels each, 1-based DMX channel numbers)
- fixture profile data (what channels a fixture mode exposes)
- fixture patch data (which profile/mode is placed at which universe/address)
- runtime parameter updates (`fixture.parameter value`)

It outputs:

- fully resolved `512`-integer DMX universe buffers, each value clamped to `0..255`. `bbb.dmx.fixturemap` can output either the selected universe as a bare 512-value list or all known universes as `universe <id> <512 values...>` messages.

This keeps `bbb.dmx.movertrack` as a low-level/test object, but `bbb.dmx.fixturemap` also owns fixture-aware tracking messages so larger patches can use loaded fixture positions, rotations, calibration, profile ranges, and universe mappings without one `movertrack` object per fixture.

---

## 2. Non-goals

Do **not** make manufacturer fixture files the internal source of truth.

GDTF, OFL, vendor PDFs, and hand-written Max patches all disagree in detail. Importers can be added later, but the runtime must use one small normalized schema that we control.

Do **not** mix fixture profile and stage patch into one flat object.

A fixture profile describes a model/mode. A fixture instance describes a patched unit in a show.

---

## 3. Core model

### 3.1 Universe

A universe is exactly `512` byte channels.

Rules:

- DMX addresses are user-facing and `1..512`.
- Internal array indices are `0..511`.
- Address overflow is an error by default.
- Optional clipping mode may be added later, but silent overflow must never be default behavior.

### 3.2 Fixture profile

A profile describes reusable fixture capabilities.

A profile contains:

- `key`: stable machine key, e.g. `generic.mover.16bit`
- `manufacturer`: display string
- `model`: display string
- `modes`: named DMX layouts

A mode contains:

- `key`: mode key, e.g. `basic16`
- `footprint`: DMX channel count
- `channels`: relative channel definitions, using 1-based offsets within the fixture footprint
- `parameters`: semantic parameter definitions mapped onto one or more channels

### 3.3 Fixture instance / patch

A patch instance contains:

- `id`: stable show-local fixture id, e.g. `spot_01`
- `profile`: profile key
- `mode`: mode key
- `universe`: integer universe id; for the first mapper object this is normally `1`
- `address`: DMX start address, `1..512`
- optional spatial/calibration data used by other `bbb.dmx.*` objects

The mapper allows multiple universes in one patch file. A single object instance can select one universe for compatibility output, or output every known universe when `@universe_mode all` or `bangall` is used.

---

## 4. Canonical file format

Use Max `dict`-compatible JSON as the canonical format.

Reason:

- Max already understands `dict` and JSON well enough.
- It is readable and diffable.
- It works cross-platform without extra dependencies.
- It can be generated from future importers.

Recommended package locations:

```text
fixtures/        # reusable profile JSON files
patches/         # show patch JSON files
```

The release package includes these folders once the mapper object exists. Profile paths in patch files are resolved relative to the patch JSON file path.

---

## 5. Profile schema

### 5.1 Minimal profile example

```json
{
  "schema": "bbb.dmx.fixture.profile.v1",
  "key": "generic.mover.16bit",
  "manufacturer": "Generic",
  "model": "16-bit Pan/Tilt Mover",
  "modes": {
    "basic16": {
      "label": "Basic 16-bit",
      "footprint": 8,
      "channels": [
        { "offset": 1, "key": "pan.coarse",  "default": 128 },
        { "offset": 2, "key": "pan.fine",    "default": 0 },
        { "offset": 3, "key": "tilt.coarse", "default": 128 },
        { "offset": 4, "key": "tilt.fine",   "default": 0 },
        { "offset": 5, "key": "dimmer",      "default": 0 },
        { "offset": 6, "key": "shutter",     "default": 0 },
        { "offset": 7, "key": "color",       "default": 0 },
        { "offset": 8, "key": "gobo",        "default": 0 }
      ],
      "parameters": {
        "pan": {
          "type": "u16",
          "channels": ["pan.coarse", "pan.fine"],
          "byte_order": "coarsefine",
          "range_degrees": 540,
          "default": 32768
        },
        "tilt": {
          "type": "u16",
          "channels": ["tilt.coarse", "tilt.fine"],
          "byte_order": "coarsefine",
          "range_degrees": 270,
          "default": 32768
        },
        "dimmer": {
          "type": "u8",
          "channel": "dimmer",
          "default": 0
        },
        "shutter": {
          "type": "u8",
          "channel": "shutter",
          "default": 0
        }
      }
    }
  }
}
```

### 5.2 Channel object

Required fields:

| Field | Type | Meaning |
|---|---|---|
| `offset` | int | Relative DMX channel, `1..footprint` |
| `key` | string | Stable channel key within the mode |

Optional fields:

| Field | Type | Default | Meaning |
|---|---|---:|---|
| `default` | int | `0` | Startup value. Channel defaults are bytes clamped `0..255`; parameter defaults are clamped to that parameter type range. |
| `label` | string | `key` | Human-readable label |
| `hold` | bool | `false` | If true, preserve previous value on reset |

### 5.3 Parameter object

Supported `type` values for v1:

| Type | Runtime value | Channel write |
|---|---|---|
| `u8` | `0..255` or normalized `0..1` | one byte |
| `u16` | `0..65535` or normalized `0..1` | two contiguous bytes |
| `u24` | `0..16777215` or normalized `0..1` | three contiguous bytes |
| `enum` | symbol or index | one byte from range table |

`byte_order` values:

| Value | Meaning |
|---|---|
| `coarsefine` | 16-bit MSB then LSB |
| `finecoarse` | 16-bit LSB then MSB |
| `coarsemidfine` | 24-bit MSB, middle, LSB |
| `finemidcoarse` | 24-bit LSB, middle, MSB |

For high-resolution color channels, the current mapper writes contiguous bytes starting at the first channel listed in `channels`. Do not describe non-contiguous 16-bit/24-bit fixture channels yet; that would be dishonest, because the implementation does not support them.

For v1, `float physical unit -> DMX` conversion should only be implemented where it is essential:

- `pan` / `tilt` may expose `range_degrees`
- generic channels should use raw byte or normalized values first

Trying to model every fixture's physical units immediately is fake precision. It will create bugs faster than value.

---

## 6. Patch schema

### 6.1 Minimal patch example

```json
{
  "schema": "bbb.dmx.patch.v2",
  "coordinates": "gdtf",
  "profiles": [
    "../fixtures/generic.mover.16bit.json"
  ],
  "fixtures": [
    {
      "id": "spot_01",
      "profile": "generic.mover.16bit",
      "mode": "basic16",
      "universe": 1,
      "address": 1,
      "position": [0.0, 0.0, 3.0],
      "rotation": [0.0, 0.0, 0.0],
      "calibration": {
        "pan_offset": 0.0,
        "tilt_offset": 0.0,
        "pan_invert": false,
        "tilt_invert": false
      }
    },
    {
      "id": "spot_02",
      "profile": "generic.mover.16bit",
      "mode": "basic16",
      "universe": 1,
      "address": 17,
      "position": [2.0, 0.0, 3.0]
    }
  ]
}
```

### 6.2 Patch validation

Loading a patch must validate:

- every fixture id is unique
- every referenced profile exists
- every referenced mode exists
- `1 <= address <= 512`
- `address + footprint - 1 <= 512` for the selected output universe
- no channel overlaps unless explicitly allowed

Default overlap policy: `error`.

Optional future policies:

- `last_wins`
- `highest_takes_precedence`
- `merge_htp_for_dimmer_only`

Do not implement these policies first. Start strict.

---

## 7. Current Max API

### 7.1 Object

```max
[bbb.dmx.fixturemap @patch patches/show.json @universe 1 @autobang 1]
```

Attributes implemented today:

| Attribute | Type | Default | Description |
|---|---|---:|---|
| `@patch` | symbol/string | empty | Patch JSON path. Loaded on initialization and by `read`. |
| `@universe` | int | `1` | Selected universe to output. Universes are 1-based. |
| `@autobang` | bool | `1` | If non-zero, successful updates immediately output according to `@universe_mode`. |
| `@universe_mode` | symbol | `selected` | Output mode for `bang` and autobang: `selected` or `all`. |
| `@tracking_mode` | symbol | `smart` | Pan-continuity mode for fixture-aware tracking: `smart`, `pan`, or `off`. |
| `@default_pan_range` | float | `540.` | Fallback pan range in degrees when a profile omits `pan.range_degrees`. |
| `@default_tilt_range` | float | `270.` | Fallback tilt range in degrees when a profile omits `tilt.range_degrees`. |
| `@track_strict` | bool | `0` | If non-zero, `trackall` errors on fixtures without pan/tilt instead of skipping them. |
| `@color_use_white` | bool | `1` | RGBW behavior for `color`/`colorall`: extract white from RGB when enabled, leave white untouched when disabled. |
| `@color_wheel_fallback` | bool | `0` | Optional fallback for fixtures without RGB/RGBW/CMY: map semantic RGB to color wheel hue plus `dimmer = max(r,g,b)` when available. |

Outlets:

1. left outlet: in `selected` mode, a bare `512` integer list; in `all`/`bangall`, one `universe <id> <512 values...>` message per known universe
2. right outlet: status/error messages

### 7.2 Messages

#### Load / inspect

```max
read patches/show.json
reload
dump
clear
reset
bang
bangall
```

- `read` loads a patch JSON path.
- `reload` reloads the current patch.
- `dump` reports current load status, selected universe, `universe_mode`, tracking settings, color white/fallback modes, and known universe ids from the right outlet.
- `clear` clears loaded profiles, patch data, and universe buffers.
- `reset` restores loaded fixture channels to profile defaults.
- `bang` outputs according to `@universe_mode`. Default `selected` mode preserves the bare 512-value list.
- `bangall` always outputs every known universe as `universe <id> <512 values...>` messages.

#### Set fixture parameter

```max
set spot_01 dimmer 255
set spot_01 pan 32768
set spot_01 tilt 32768
set spot_01 pan_tilt 32768 32768
```

`set` first attempts the parameter as a 16-bit value and falls back to 8-bit where appropriate. `pan_tilt` is a convenience pseudo-parameter for setting both 16-bit pan and tilt in one message.

#### Normalized input

```max
nset spot_01 dimmer 1.0
nset spot_01 pan 0.5
nset spot_01 tilt 0.5
```

`nset` clamps `0.0..1.0` and maps onto the target parameter's DMX range: `u8` to `0..255`, `u16` to `0..65535`, and `u24` to `0..16777215`.

#### Semantic color input

```max
color spot_01 rgb 1.0 0.8 0.0
colorall rgb 1.0 0.8 0.0
colorall rgb8 255 204 0
```

`color` and `colorall` express the desired additive RGB color instead of raw fixture parameter names. `rgb` values are normalized `0.0..1.0`; `rgb8` values are `0..255`. RGB fixtures receive `red`, `green`, and `blue`. If an RGB/RGBW/CMY color block has a preceding dimmer/intensity parameter, semantic color uses the nearest preceding dimmer as brightness `max(r,g,b)` and normalizes the color channels by that brightness. This avoids opening earlier non-color dimmers on fixtures such as strobe hybrids. RGBW fixtures receive `red`, `green`, `blue`, and extracted `white = min(red, green, blue)` when `@color_use_white 1`. With `@color_use_white 0`, RGBW fixtures receive full `red`, `green`, and `blue`; `white` is left untouched so it can be managed separately. CMY fixtures receive subtractive `cyan = 1 - red`, `magenta = 1 - green`, and `yellow = 1 - blue`.

Color wheel fallback is deliberately opt-in. With `@color_wheel_fallback 1`, only fixtures that have no RGB/RGBW/CMY semantic model fall back to a color wheel plus dimmer model. The requested RGB is split into brightness `max(r,g,b)` and normalized hue `rgb / brightness`; black (`0 0 0`) uses open/white hue with brightness `0`. Matching uses `parameter.wheel` / `range.wheel_slot` against top-level `wheels[].slots[].rgb` or `cie_xyY`; if those are absent it falls back to conservative color-name matching from range/slot labels. The color wheel value is the midpoint of the matched range; if a `dimmer` parameter exists, it is set to the brightness. When multiple ranges are equally good matches, current wheel raw value is used as a tie-breaker so the nearest equivalent open/slot is chosen. With the default `@color_wheel_fallback 0`, semantic color never touches a color wheel.

`colorall` skips fixtures that do not expose a supported RGB/RGBW/CMY model or, when enabled, a supported color wheel fallback. Per-fixture `color` reports an error for unsupported fixtures. This behavior is deliberately separate from `setall`/`nsetall`, which only write same-named parameters and do not perform color-model conversion.

#### Semantic dimmer input

```max
dimmer spot_01 1.0
dimmerall 0.5
```

`dimmer` and `dimmerall` express desired fixture intensity instead of a raw parameter name. For RGB/RGBW/CMY fixtures with a color-block dimmer, this writes the nearest preceding dimmer/intensity parameter for the color block. If no color-block dimmer exists, it falls back to the fixture's `dimmer` parameter, then the first dimmer/intensity-like parameter. Use raw `nset`/`nsetall` when you intentionally need a specific fixture parameter such as `dimmer_2` or a strobe/beam dimmer.

#### Semantic shutter input

```max
shutter spot_01 1
shutter spot_01 0
shutterall 1
shutterall 0
```

`shutter` and `shutterall` express desired shutter state instead of raw `shutter`, `shutter-strobe`, or `strobe` DMX values. Numeric `1`/non-zero means open; `0` means close. Symbols `open`, `on`, `true`, `close`, `closed`, `off`, `false`, and `blackout` are also accepted.

Resolution order is deliberately simple and deterministic:

1. On likely shutter/strobe parameters, prefer range metadata whose `function` or `label` is `open` for opening, or `closed`/`close`/`blackout` for closing. Write the midpoint of the matching range.
2. For GDTF-style split strobe fixtures, parameters whose channel label identifies `StrobeMode...` are reset to a `No effect` / `No function` range if present for both open and close. `StrobeDuration`, `StrobeRate`, `StrobeFrequency`, and `StrobeSpeed` parameters are reset to their profile defaults instead of being treated as open fallbacks.
3. Prefer parameters named like `shutter`, then `shutter-strobe`, then other names containing `shutter`, then names containing `strobe`.
4. If closing and no explicit shutter closed/blackout range exists, write any dimmer/intensity parameters with a closed range to closed.
5. If no matching range exists but a likely non-rate/non-duration shutter/strobe parameter exists, write max (`255`, `65535`, or `16777215`) for open and `0` for close.

If shutter and strobe share the same channel, the semantic shutter write intentionally overwrites that channel. Fixtures with multiple shutter/strobe mode channels may receive multiple writes for one semantic shutter command. `shutterall` skips fixtures without a supported shutter/strobe parameter; per-fixture `shutter` reports an error for unsupported fixtures.

#### Raw channel override

```max
channel 1 255
channels 1 255 2 128 3 0
```

Raw override is implemented for testing and emergency control. It should stay visibly separate from fixture parameters; otherwise patches become unreadable.

#### Fixture-aware mover tracking

`bbb.dmx.fixturemap` supports direct tracking messages:

```max
track spot_01 target_x target_y target_z
trackall target_x target_y target_z
trackrel spot_01 relative_x relative_y relative_z
trackallrel relative_x relative_y relative_z
trackreset [spot_01]
```

`track` uses the loaded patch fixture `position`, `rotation`, and `calibration` values, plus the target fixture mode's semantic `pan` and `tilt` parameters. The pan/tilt ranges come from `parameters[].range_degrees`; missing ranges fall back to `@default_pan_range` and `@default_tilt_range`. The resulting 16-bit normalized pan/tilt values are written through the fixture profile, so `u8`, `u16`, and `u24` parameter widths keep their declared byte order.

`trackall` applies the same world-space target to every mover in the patch. Non-mover fixtures are skipped by default; set `@track_strict 1` to turn those skips into errors. `trackrel` and `trackallrel` interpret the vector relative to each fixture origin. `trackreset` clears stored pan-continuity history used by `@tracking_mode smart|pan|off`.

`bbb.dmx.movertrack` remains useful for small projects, tests, and hand-built Max patches. Its byte tuple can still be routed into fixturemap:

```max
ptbytes spot_01 pan1 pan2 tilt1 tilt2
```

`ptbytes` combines the incoming pan/tilt bytes using the target fixture profile's `byte_order`. There is no separate `set16` message in the current object; use `set spot_01 pan_tilt pan_u16 tilt_u16` for semantic 16-bit values.

---

## 8. Output behavior

Current behavior:

- Every successful value update updates the internal multi-universe buffer.
- If `@autobang 1`, successful `read`, `reload`, `set`, `nset`, `color`, `colorall`, `dimmer`, `dimmerall`, `shutter`, `shutterall`, `track`, `trackall`, `trackrel`, `trackallrel`, `ptbytes`, `channel`, `channels`, `clear`, and `reset` operations output according to `@universe_mode`.
- `@universe_mode selected` outputs the selected full 512-byte universe as a bare list. This is the default compatibility mode.
- `@universe_mode all` outputs one `universe <id> <512 values...>` message per known universe.
- `bang` follows `@universe_mode`; `bangall` forces all-universe output.
- `reset` restores profile defaults for loaded fixtures.
- `clear` leaves the object empty and outputs a zeroed selected universe when `@autobang 1`; in all mode with no known universes it outputs the selected zeroed universe as a `universe` message.

Not implemented today:

- `@dirty_only`
- `@include_selector`
- changed-channel delta output

Those are future optimizations. The simple full-universe list is the compatibility contract for v1 because many Max patches prefer a single list.

---

## 9. Error policy

This object is strict and loud during load, quiet during high-rate runtime updates.

Load-time errors:

- bad JSON/dict
- missing profile
- bad mode
- footprint overflow
- overlap

Runtime warnings should be once-per-condition where practical:

- unknown fixture id
- unknown parameter
- out-of-range value clamped
- non-finite number ignored

Invalid runtime updates must not corrupt the universe buffer.

---

## 10. Current shared C++ layer

The Max-independent implementation lives under `source/bbb/dmx/`:

```text
common.hpp
frame_set.hpp
fixture_json.hpp
fixture_mapper.hpp
fixture_patch.hpp
fixture_profile.hpp
math.hpp
universe.hpp
value.hpp
```

Core responsibilities:

- `universe.hpp` defines a strict 512-channel DMX universe.
- `frame_set.hpp` defines a multi-universe frame set shared by utility externals.
- `fixture_profile.hpp` and `fixture_patch.hpp` define profile/mode/parameter/patch data structures.
- `fixture_mapper.hpp` owns strict mapping, defaults, raw channel writes, `set_u8`, `set_u16`, `set_u24`, `set_normalized`, and `set_pan_tilt_bytes`.
- `fixture_json.hpp` loads the normalized JSON profile/patch files and resolves profile paths relative to the patch file.
- `value.hpp` provides byte-order helpers shared with `movertrack`.

The JSON parser is intentionally separate from the pure mapping path so unit tests can construct fixtures directly without file I/O.

---

## 11. Implementation status / remaining work

Implemented:

1. `dmx_universe` and low-level write helpers.
2. Profile/mode/parameter data structures.
3. Strict patch validation.
4. JSON patch/profile loading.
5. `fixture_mapper` unit tests.
6. `bbb.dmx.fixturemap` external.
7. Help patch and sample `fixtures/` + `patches/` data.
8. Patch validation/inspection utilities: `bbb.dmx.patchcheck` and `bbb.dmx.fixtureinfo`.
9. Multi-universe utility layer documented in `docs/SPEC.utilities.md`.

Remaining future work:

- Optional importers from GDTF/OFL/vendor data into the normalized JSON schema.
- Higher-level fixture editors. Do not start with this until the deterministic mapper API has more real-show mileage.
- Optional dirty/delta outlet if full-universe output becomes a measured performance problem.

---

## 12. Design decision

Canonical fixture data should be specified as two JSON/dict layers:

1. reusable fixture profiles in `fixtures/*.json`
2. show-specific fixture patches in `patches/*.json`

This is the lowest-friction path for Max users while still being strict enough to support future importers and CI tests.
