# zmk-bootloader-hold

A ZMK devicetree module: `&bootloader_hold`, a hold-tap that requires a real
3-second hold before `&bootloader` fires, so a keyboard can't be accidentally
dropped into UF2 bootloader mode by a stray combo or a moment of resting a
finger on the wrong key. A quick tap does nothing.

An optional second piece, `zmk,bootloader-warn`, raises a ZMK event partway
through the hold (before the reboot actually fires), so a keyboard with a
display or LEDs can react — this module doesn't ship any display code
itself, since that's inherently specific to your own hardware, but the
[docs](docs/) walk through a full real example.

Extracted from [wireless-klor-zmk-config](https://github.com/vzhny/wireless-klor-zmk-config),
where getting this right (a `#binding-cells` gotcha, and confirming ZMK's
per-half routing on a split survives being wrapped in a hold-tap) took more
research than it should have. This module exists so that work doesn't need
repeating on the next keyboard.

## Quickstart

Add to `config/west.yml`:

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

In your `.keymap`:

```dts
#include <behaviors/bootloader_hold.dtsi>
```

Bind it anywhere with two dummy args (required but ignored — see
[`bootloader_hold.dtsi`](dts/behaviors/bootloader_hold.dtsi) for why):

```dts
&bootloader_hold 0 0
```

That's a working 3-second-hold-to-reboot key. It's also almost certainly
not what you actually want on a split keyboard — see the docs below for
per-half routing, why combos break it, and a full real worked example.

## Docs

- **[Module setup](docs/module-setup.md)** — installation in more depth,
  and what each of the two pieces (`bootloader_hold` /
  `zmk,bootloader-warn`) actually does and doesn't do.
- **[Split-layer recipe](docs/split-layer-recipe.md)** — building a layer
  that's reachable independently on each half of a split keyboard, hard to
  trigger by accident, using KLOR's actual devicetree as a full worked
  example. Covers why combos break per-half routing, and the reboot-order
  caveat that matters once you have this working.
- **[Display integration](docs/display-integration.md)** — reacting to the
  `zmk_bootloader_warning` event with real display code (KLOR's actual
  widget code, both the LVGL visibility-swap technique and the
  split-communication problem of a peripheral half's own display).

## License

MIT, see [LICENSE](LICENSE).
