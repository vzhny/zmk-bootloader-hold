# zmk-bootloader-hold

A ZMK devicetree module: `&bootloader_hold`, a hold-tap that requires a real
3-second hold before `&bootloader` fires, so a keyboard can't be accidentally
dropped into UF2 bootloader mode by a stray combo or a moment of resting a
finger on the wrong key. A quick tap does nothing.

Extracted from [wireless-klor-zmk-config](https://github.com/vzhny/wireless-klor-zmk-config),
where getting this right (a `#binding-cells` gotcha, and confirming ZMK's
per-half routing on a split survives being wrapped in a hold-tap) took more
research than it should have. This module exists so that work doesn't need
repeating on the next keyboard.

## What this does and doesn't cover

**Does:** the hold-tap primitive itself — the timing, the no-accidental-tap
behavior, and (on a split board) correct per-half reboot routing as long as
you bind it directly to real keymap positions (see "Split keyboards" below).

**Doesn't:** where in your keymap this lives. Every keyboard has different
free layers and positions, so there's no generic "just add this layer" — the
[Worked example](#worked-example-an-arm--bootloader-layer-pair) below is a
pattern to adapt, not a drop-in layer.

**Doesn't (yet):** any display/LED feedback while the hold is in progress.
If your keyboard has a screen, you're on your own for wiring that up right
now — see [Roadmap](#roadmap).

## Installation

Add this repo to your `config/west.yml` manifest:

```yaml
manifest:
  remotes:
    - name: vzhny
      url-base: https://github.com/vzhny
    # ...your existing remotes (zmkfirmware, etc.)
  projects:
    - name: zmk-bootloader-hold
      remote: vzhny
      revision: main  # pin to a tag once one exists, don't track a branch long-term
    # ...your existing projects (zmk, etc.)
  self:
    path: config
```

Then in your `.keymap`:

```dts
#include <behaviors/bootloader_hold.dtsi>
```

## Usage

`bootloader_hold` takes 2 dummy arguments at every call site (`0 0`) because
ZMK's hold-tap devicetree binding hardcodes `#binding-cells = <2>` — see the
comment in [`bootloader_hold.dtsi`](dts/behaviors/bootloader_hold.dtsi) for
why. Both args are ignored; `&bootloader`/`&none` take no parameters of
their own.

```dts
&bootloader_hold 0 0
```

Bind that wherever you want a 3-second-hold-to-reboot key. That's the whole
API surface.

## Split keyboards: bind it directly, not through a combo

If you want per-half bootloader entry — holding a key physically on the
right half reboots only the right half, not the left — `bootloader_hold`
must be bound at a real keymap position that ZMK resolves directly, not
through a `zmk,combos` chord. Combos always tag their synthesized event as
locally-sourced, which makes `&bootloader` (and therefore `bootloader_hold`)
always reboot the central regardless of which half's keys formed the combo.
Use a conditional-layers chord or a dedicated layer entered via `&mo`/`&lt`
to reach the position instead — see the worked example below, and the long
comment in the `.dtsi` file for the full technical reasoning.

If you don't care about per-half routing (unibody board, or you're fine with
combos always targeting the central), a combo works fine — this constraint
only matters for split per-half correctness.

## Worked example: an Arm + Bootloader layer pair

This is the pattern `wireless-klor-zmk-config` actually uses, adapted as a
template. **Layer numbers and key positions below are placeholders — replace
them with whatever's free on your own keymap.**

```dts
/ {
    conditional_layers {
        compatible = "zmk,conditional-layers";

        /* Holding two existing momentary layers together "arms" one key
         * with &mo BOOTLOADER, without needing a dedicated combo or extra
         * physical position for entry. Pick two layers/positions on your
         * own keymap that are safely unlikely to be held together during
         * normal use. */
        arm {
            if-layers = <NUM_LAYER NAV_LAYER>;
            then-layer = <ARM_LAYER>;
        };
    };

    keymap {
        /* ARM_LAYER: only interesting binding is at one position, arming
         * it with &mo BOOTLOADER_LAYER. Everything else &trans. */
        arm_layer {
            bindings = <
                ... &trans &mo BOOTLOADER_LAYER &trans ...
            >;
        };

        /* BOOTLOADER_LAYER: only reachable by continuing to hold the armed
         * key (ZMK resolves a key's binding once, at its own press, from
         * whatever layers were active at that instant -- releasing the
         * two ARM trigger layers afterward doesn't undo this, the armed
         * key's own hold is what keeps BOOTLOADER_LAYER open). Bind
         * bootloader_hold at one position per half you want independently
         * rebootable. Everything else &trans. */
        bootloader_layer {
            bindings = <
                &bootloader_hold 0 0  &trans  ...  &trans  &bootloader_hold 0 0
            >;
        };
    };
};
```

Why arm via two *already-existing* layers instead of a dedicated combo: it
costs no extra physical position, and — per the split note above — keeps
the eventual `&bootloader_hold` invocation as a direct position binding
rather than something routed through the combo system.

## Roadmap

A second, opt-in piece: a small listener that fires a ZMK event partway
through the hold (e.g. at 1 second), so a keyboard with a display/LEDs can
react before the actual reboot fires at 3 seconds, without this module
needing to know anything about your hardware. Not built yet — the
`wireless-klor-zmk-config` version of this currently does it with bespoke,
project-specific code (its own BLE channel to notify the peripheral half),
which doesn't generalize as-is. Tracked, not started.

## License

MIT, see [LICENSE](LICENSE).
