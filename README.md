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

**Doesn't:** deciding what to actually show on a display or LED. The
optional `zmk,bootloader-warn` node (below) tells you *when* to react via a
ZMK event — writing the actual display/LED code that reacts to it is still
yours, since that's inherently specific to your own hardware.

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
      revision: v1.1.0  # pin to a tag, don't track main long-term
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

    /* Optional -- see "Reacting before the reboot fires" below. One node
     * per watched position, same POSITION_LEFT/POSITION_RIGHT/
     * BOOTLOADER_LAYER placeholders as above. */
    bootloader_warn_left: bootloader_warn_left {
        compatible = "zmk,bootloader-warn";
        layer = <BOOTLOADER_LAYER>;
        position = <POSITION_LEFT>;
        latch;
    };
    bootloader_warn_right: bootloader_warn_right {
        compatible = "zmk,bootloader-warn";
        layer = <BOOTLOADER_LAYER>;
        position = <POSITION_RIGHT>;
        latch;
    };
};
```

Why arm via two *already-existing* layers instead of a dedicated combo: it
costs no extra physical position, and — per the split note above — keeps
the eventual `&bootloader_hold` invocation as a direct position binding
rather than something routed through the combo system.

## Reacting before the reboot fires

`bootloader_hold` itself has no hook for "partway through the hold" — it's
a plain hold-tap, it only ever decides tap-or-hold at the end. If you want
a display or LED to react before the actual 3-second reboot (e.g. an
unmistakable warning that continuing will reboot that half), add a
`zmk,bootloader-warn` node per watched position:

```dts
/ {
    bootloader_warn_left: bootloader_warn_left {
        compatible = "zmk,bootloader-warn";
        layer = <BOOTLOADER_LAYER>;    /* only arms while this layer is active */
        position = <POSITION_LEFT>;    /* same position bound to &bootloader_hold */
        warn-after-ms = <1000>;        /* default is 1000 if omitted */
        latch;                         /* see below */
    };
};
```

This raises a `zmk_bootloader_warning` event (`position`, `layer`, `active`,
`timestamp`) that your own project subscribes to like any other ZMK event —
no special cross-module wiring needed beyond including the header:

```c
#include <bootloader_hold/events.h>

static int my_display_cb(const zmk_event_t *eh) {
    const struct zmk_bootloader_warning *ev = as_zmk_bootloader_warning(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->active) {
        /* show your warning */
    } else {
        /* only reachable if `latch` is unset on the node -- revert it */
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(my_display, my_display_cb);
ZMK_SUBSCRIPTION(my_display, zmk_bootloader_warning);
```

`latch`, on the devicetree node, decides what happens if the position is
released before `bootloader_hold`'s own `tapping-term-ms` elapses (i.e. the
real reboot never fires): with `latch` set, the event never fires with
`active=false` — once warned, your listener should treat it as permanent
until you decide otherwise (this is what `wireless-klor-zmk-config` does:
the display stays stuck until an actual power cycle, as a deliberate signal
that *something* happened, even if it wasn't a real reboot). Without
`latch`, an `active=false` event fires immediately on release, and your
listener is expected to revert whatever it did.

This node only detects and announces; it has no effect on whether
`&bootloader_hold` actually fires or not, and no dependency on it beyond
watching the same position/layer — the two are configured independently.

## License

MIT, see [LICENSE](LICENSE).
