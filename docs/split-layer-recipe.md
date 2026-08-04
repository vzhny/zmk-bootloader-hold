# Setting up a per-half-reachable layer

This is the part the module can't ship for you — every keyboard has
different free layers and positions, so there's no drop-in "bootloader
layer." What follows is the actual pattern
[wireless-klor-zmk-config](https://github.com/vzhny/wireless-klor-zmk-config)
uses, in full, with the reasoning behind each piece, so you can adapt it
rather than reverse-engineer it.

## The goal

On a split keyboard, `&bootloader` reboots whichever *physical half a
keypress actually originated from* — not always the central — as long as
it's reached through a direct keymap position binding. Holding a key
that's physically on the right half can independently reboot the right
MCU, leaving the left one alone, and vice versa. This lets you flash each
half from a fully wireless, battery-powered pair with no physical reset
button.

Two requirements make this safe:

1. **Hard to trigger by accident.** A stray combo or a resting finger
   should never come close to rebooting a half of your keyboard.
2. **A real binding, not a combo.** Combos in ZMK always tag their
   synthesized event as locally-sourced (`combo.c`:
   `.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL`), which means a
   combo-bound `&bootloader` *always* reboots the central, no matter which
   half's keys formed the combo. If per-half routing matters to you, the
   position that's actually bound to `&bootloader_hold` has to be resolved
   as an ordinary keymap position press — not a combo.

## The shape: arm, then hold

KLOR's answer to requirement 1 without breaking requirement 2: use two
*already-active* layers (each with its own existing hold-key, costing no
new physical position) to "arm" a third key via `conditional-layers`, and
have that third key's own continued hold be what keeps a dedicated
Bootloader layer open — decoupled from whether the original two layers are
still held.

This works because of how ZMK resolves a key's binding: **once, at the
moment it's pressed**, using whatever layers were active at that instant
(`zmk_keymap_position_state_changed` snapshots `_zmk_keymap_layer_state`
into `zmk_keymap_active_behavior_layer[position]` on press, and reuses that
same snapshot on release). So: press the arming key while the two trigger
layers are active, and its binding — `&mo <bootloader-layer>` — is locked
in for as long as *that key* stays held, regardless of what happens to the
two trigger layers afterward.

Concretely, from `klor.keymap`:

```dts
/ {
    conditional_layers {
        compatible = "zmk,conditional-layers";

        /* Holding NUM (mo 4) + NAV (lt 5, via Z) "arms" V (position 24)
         * with &mo 10. Releasing Num/Nav afterward does NOT drop this --
         * see the binding-resolution note above. */
        arm {
            if-layers = <4 5>;
            then-layer = <9>;
        };
    };

    keymap {
        /* ARM (layer 9) -- only interesting binding is at V's position,
         * arming it with &mo 10. Everything else &trans. */
        arm_layer {
            bindings = <
                &trans  &trans  &trans  &trans  &trans        &trans  &trans  &trans  &trans  &trans
                &trans  &trans  &trans  &trans  &trans        &trans  &trans  &trans  &trans  &trans
                &trans  &trans  &trans  &trans  &mo 10  &trans  &trans   &trans  &trans  &trans  &trans  &trans
                                &trans  &trans  &trans          &trans  &trans  &trans
            >;
        };

        /* BOOTLOADER (layer 10) -- only reachable by continuing to hold V.
         * Q and `;` each independently need their own 3-second hold. */
        bootloader_layer {
            bindings = <
                &bootloader_hold 0 0  &trans  &trans  &trans  &trans        &trans  &trans  &trans  &trans  &bootloader_hold 0 0
                &trans                &trans  &trans  &trans  &trans        &trans  &trans  &trans  &trans  &trans
                &trans                &trans  &trans  &trans  &trans  &trans  &trans   &trans  &trans  &trans  &trans  &trans
                                      &trans  &trans  &trans          &trans  &trans  &trans
            >;
        };
    };
};
```

Physically: hold the Num thumb key + `Z` briefly, press-and-hold `V` (this
arms it), release the Num thumb key and `Z` — `V` alone now keeps the
Bootloader layer open for as long as it stays held. Then, *while still
holding `V`*, holding `Q` for 3 seconds reboots the left/central half;
holding `;` for 3 seconds reboots the right/peripheral half (`Q` is
position 0, always physically on the left in KLOR's matrix transform; `;`
is position 9, always physically on the right).

## Adapting this to your own keyboard

Everything in the example above is a placeholder for values specific to
KLOR's layout:

- **The two arming layers** (`4`, `5` — Num and Nav in KLOR) can be any two
  layers you already have, as long as their own trigger keys are unlikely
  to be held together during normal typing. They cost nothing extra
  precisely because they already exist for other reasons.
- **The armed position** (`24`/`V` in KLOR) can be any position not
  otherwise meaningful on the arm layer — it becomes `&mo <bootloader
  layer>` there and nothing else.
- **The Bootloader layer number** (`10`) just needs to be free and,
  ideally, higher than every other conditional/momentary layer your board
  uses, for the same reason ZMK resolves highest-active-layer-first
  everywhere else.
- **Which positions get `&bootloader_hold`** — bind one per physical half
  you want independently rebootable. On a 3+ way split you'd bind one per
  physical MCU. On a unibody board, one is enough (see
  [module-setup.md](module-setup.md) for the simpler non-split form).

## Reboot order matters

Reboot whichever position maps to a **peripheral** half before whichever
position maps to the **central**. Once the central reboots, it stops
running ZMK entirely — no keymap, no BLE central role — so a peripheral
that's still waiting for its own reboot has nothing left to resolve
anything through. There's no recovering from this except a physical
reset on that peripheral.

Don't hold two `&bootloader_hold` positions at once, either, even if
you've got the order right in principle. If both terms elapse together,
the central's own reboot can cut its BLE radio before the "reboot
yourself" instruction to a peripheral has actually finished transmitting
over the air, leaving that peripheral un-rebooted with no indication why.
Do them strictly one at a time, peripherals fully confirmed rebooted
before the central.

## Next

Once this is wired up, [display-integration.md](display-integration.md)
covers reacting to it with a "BOOTLOADER" display warning before the
actual reboot fires, using KLOR's real widget code as the example.
