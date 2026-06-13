# SPEC.utilities.md — multi-universe DMX utility objects

## 1. Scope

This document covers non-output `bbb.dmx.*` utilities. DMX network transmission is intentionally outside this repository; use `bbb.artnet` or another sender after these objects.

## 2. Canonical stream format

Utility objects use explicit multi-universe messages:

```text
universe <id> <512 byte values>
```

Rules:

- Universe ids are 1-based. Values below `1` are sanitized to `1`.
- DMX channel addresses are 1-based `1..512`.
- Byte values are clamped to `0..255`.
- Bare `list` input, where supported, is shorthand for the object's default `@universe` only. It is not the canonical multi-universe boundary format.

## 3. Implemented objects

### `bbb.dmx.monitor`

Stores the latest known frame per universe and can emit either full frames or changed-channel deltas.

- Input: `list`, `universe`, `channel`, `channels`, `bang`, `bangall`, `dump`, `clear`
- Output: `universe <id> ...` or `changed <id> address value ...`
- Attributes: `@universe`, `@changed_only`

### `bbb.dmx.merge`

Merges named layers across multiple universes.

- Input: `universe layer id ...`, `layer layer id ...`, `channel`, `channels`, `priority`, `clear`, `bang`, `bangall`, `dump`
- Output: merged `universe <id> ...`
- Modes: `priority`, `htp`, `ltp`

`priority` is the deterministic default for show-control patches. `htp` is useful for intensities. `ltp` is useful for controller takeover, but it is easier to misunderstand because recent zero values are still intentional values.

### `bbb.dmx.fade`

Interpolates full frames and channel updates over time.

- Input: `list`, `universe id time_ms ...`, `channel id address value time_ms`, `channels id time_ms address value ...`, `bang`, `bangall`, `stop`, `clear`
- Output: timer-driven `universe <id> ...`
- Attributes: `@universe`, `@time_ms`, `@fps`

### `bbb.dmx.record`

Records and plays snapshots of the full known multi-universe frame set.

- Input: `list`, `universe`, `channel`, `channels`, `record`, `play`, `frame`, `write`, `read`, `bang`, `bangall`, `dump`, `clear`
- Output: `universe <id> ...` during input, frame recall, and playback
- Attributes: `@universe`, `@fps`, `@loop`

The record file is a simple line-oriented text format:

```text
bbb.dmx.record 1
frame <index> universe <id> <512 byte values>
```

### `bbb.dmx.generator`

Generates deterministic test frames.

- Input: `blackout`, `full`, `all`, `range`, `ramp`, `chase`
- Output: `universe <id> ...`
- Attribute: `@universe`

### `bbb.dmx.safety`

Applies guardrails before output handoff.

- Input: `list`, `universe`, `channel`, `channels`, `blackout`, `freeze`, `bang`, `bangall`
- Output: guarded `universe <id> ...`
- Attributes: `@universe`, `@max_value`, `@max_delta`, `@timeout_ms`, `@blackout`, `@freeze`

This should sit late in the chain, immediately before the sender. It is not a substitute for physical E-stop or fixture power control.

### `bbb.dmx.patchcheck`

Validates fixture patch/profile JSON.

- Input: `read`, `bang`
- Output: `ok fixtures <count> universes <id...>` or `error <message>`

### `bbb.dmx.fixtureinfo`

Reports fixture metadata from loaded patch/profile JSON.

- Input: `read`, `bang`, `listfixtures`, `fixture`, `listparams`, `modeparams`, `param`, `dump`
- `listparams fixture_id` / `param fixture_id parameter_key` inspect a patched fixture instance.
- `listparams profile_key mode_key`, `modeparams profile_key mode_key`, and `param profile_key mode_key parameter_key` inspect a loaded profile/mode directly.
- Output selectors: `summary`, `fixture`, `param`, `error`

### `bbb.dmx.matrixmap`

Samples `char` or `float32` `jit.matrix` color data into fixture parameters through the common fixture patch/profile layer.

- Input: `jit_matrix`, `readpatch`, `readmap`, `reload`, `bang`, `dump`
- Output: `universe <id> <512 byte values>`
- Attributes: `@patch`, `@map`, `@universe`, `@universe_mode`, `@plane_order`, `@gamma`, `@brightness`, `@autobang`, `@invert_x`, `@invert_y`
- Matrix values are normalized first, then expanded through the fixture parameter depth (`u8`, `u16`, or `u24`).
- Map files live in `maps/`; see `docs/SPEC.matrixmap.md`.

`changed` mode outputs full universe frames for universes whose data changed. It is not a per-channel delta protocol.

### `bbb.dmx.palette`

Applies named normalized parameter palettes through fixture patch/profile metadata.

- Input: `readpatch`, `readpalette`, `reload`, `apply`, `clear`, `bang`, `dump`
- Output: `universe <id> <512 byte values>` for all universes used by the patch
- Attributes: `@patch`, `@palette`, `@autobang`
- Config file: `palettes/*.json` with `palettes` object.
- `apply palette_name [fixture_pattern ...]` filters fixture ids with `*`/`?` wildcards.

Palette values are normalized `0.0..1.0` and are written through fixture parameter depth (`u8`, `u16`, `u24`). Unknown parameters on a fixture are skipped so broad color palettes can target mixed fixture sets.

### `bbb.dmx.scene`

Recalls named fixture-pattern looks through fixture patch/profile metadata.

- Input: `readpatch`, `readscene`, `reload`, `apply`, `bang`, `dump`
- Output: `universe <id> <512 byte values>` for all universes used by the patch
- Attributes: `@patch`, `@scene`, `@autobang`
- Config file: `scenes/*.json` with `scenes` object.
- `apply scene_name` resets patched fixture channels to defaults, then applies scene rules.

### `bbb.dmx.fixtureview`

Decodes current raw DMX frames through fixture patch/profile metadata.

- Input: `list`, `universe`, `channel`, `channels`, `read`, `reload`, `bang`, `listfixtures`, `fixture`, `listparams`, `param`
- Output selectors: `summary`, `fixture`, `param`, `error`
- Attributes: `@patch`, `@universe`
- `param fixture_id parameter_key` reports parameter type, universe, DMX addresses, raw bytes, combined raw value, and normalized value.

### `bbb.dmx.curve`

Applies channel/range transfer curves to explicit multi-universe DMX frames.

- Input: `list`, `universe`, `channel`, `channels`, `read`, `reload`, `clear`, `gamma`, `bang`, `bangall`, `dump`
- Output: curved `universe <id> <512 byte values>`
- Attributes: `@config`, `@universe`, `@autobang`
- Config file: `curves/*.json` with `rules[]`.
- Rule fields: `universe` (`0` = all), `start`, `count` or `end`, `curve`, `gamma`, `threshold`, `points`.

Supported curves: `linear`, `gamma`, `invert`, `threshold`, and `points` (`[[input, output], ...]`, normalized `0.0..1.0`). Rules are applied in array order, so overlapping rules are intentional and deterministic.

### `bbb.dmx.mask`

Applies hard channel/range constraints to explicit multi-universe DMX frames.

- Input: `list`, `universe`, `channel`, `channels`, `read`, `reload`, `clear`, `mute`, `hold`, `allow`, `force`, `bang`, `bangall`, `dump`
- Output: masked `universe <id> <512 byte values>`
- Attributes: `@config`, `@universe`, `@autobang`
- Config file: `masks/*.json` with `rules[]`.
- Rule fields: `universe` (`0` = all), `start`, `count` or `end`, `action`, `value`.

Actions: `mute` outputs zero, `hold` keeps the previous output value, `force` outputs a fixed byte, and `allow` switches the object into whitelist mode where non-allowed channels output zero.

## 4. Chain example

A realistic Max chain should be explicit about universe boundaries:

```text
fixturemap / matrixmap / generator / controllers
  -> merge
  -> fade
  -> safety
  -> bbb.artnet sender
```

Do not hide universe identity in a plain 512-value list once the patch grows beyond one universe. That is how you create show-day bugs that look random but are actually bad data contracts.
