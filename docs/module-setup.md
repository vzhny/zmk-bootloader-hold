# Module setup

This covers installing the module and understanding its two independent
pieces. It doesn't cover *where in your keymap* to put the hold-tap, or
*what to show* on a display — those are [split-layer-recipe.md](split-layer-recipe.md)
and [display-integration.md](display-integration.md).

## Install

Add a remote and a project entry to your `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: vzhny
      url-base: https://github.com/vzhny
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main # or whatever you're already pinned to
      import: app/west.yml
    - name: zmk-bootloader-hold
      remote: vzhny
      revision: v1.1.0 # pin to a tag, don't track main long-term
  self:
    path: config
```

Pin to a tag, not `main`. This module's own commit history includes at
least one real regression that only broke a *peripheral* build (a raw
commit briefly had `bootloader_warn.c` calling a central-only ZMK
function unconditionally — see the `fix: guard bootloader_warn.c` commit),
caught by CI on a consuming project, not by anything in this repo alone.
Tracking `main` means inheriting whatever state that branch happens to be
in when your own CI runs.

Then in your `.keymap`:

```dts
#include <behaviors/bootloader_hold.dtsi>
```

That's the whole install. No Kconfig to enable, nothing else to opt into —
`bootloader_hold` is inert until you actually bind it somewhere, and the
optional `zmk,bootloader-warn` listener (below) is inert until you declare
a devicetree node for it.

## The two pieces, and how they relate

**`&bootloader_hold`** — a hold-tap. Bind it at a keymap position; hold
that position for 3 seconds and `&bootloader` fires; anything shorter and
nothing happens. This is the only piece that actually reboots anything.

**`zmk,bootloader-warn`** (optional) — a devicetree node, not something you
bind in your keymap bindings array. It watches a position + a layer from
the *outside* and raises a `zmk_bootloader_warning` ZMK event after a
configurable delay (default 1000ms) — meant to fire *before*
`bootloader_hold`'s own 3-second term elapses, so you get a chance to react
(a display warning, an LED, whatever) before the actual reboot. It has **no
effect** on `bootloader_hold` and no dependency on it beyond conventionally
watching the same position and layer — you could use one without the
other, though pairing them is the whole point.

Minimal non-split example — one key, no display, no per-half concerns,
just the hold-tap on its own:

```dts
#include <behaviors/bootloader_hold.dtsi>

/ {
    keymap {
        compatible = "zmk,keymap";
        default_layer {
            bindings = <
                &kp A &kp B &bootloader_hold 0 0 ...
            >;
        };
    };
};
```

`&bootloader_hold 0 0` — the two `0 0` args are required but ignored (see
the comment in [`bootloader_hold.dtsi`](../dts/behaviors/bootloader_hold.dtsi)
for why: ZMK's hold-tap devicetree binding hardcodes 2 cells regardless of
what the hold/tap children actually take).

This single-key form works on a unibody board or when you don't care which
physical half a split's reboot targets — it always reboots whichever half
resolves the keymap (the central, on a split). If you *do* care about
per-half routing, or you want the position genuinely hard to hit by
accident (not just "some key somewhere"), see
[split-layer-recipe.md](split-layer-recipe.md) for the pattern this module
was actually extracted from.
