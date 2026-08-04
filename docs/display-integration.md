# Display integration

`zmk,bootloader-warn` raises an event; it draws nothing. Writing the
"BOOTLOADER" text — or an LED color, or anything else — is deliberately
left to you, since it's inherently specific to your own hardware. This
walks through KLOR's actual implementation as a concrete example, plus the
one part of it that's genuinely split-architecture-specific and won't
apply to every board.

Prerequisite: a `zmk,bootloader-warn` node declared per watched position,
as covered in [split-layer-recipe.md](split-layer-recipe.md). Everything
below assumes that's already in place.

## 1. Subscribe to the event

```c
#include <bootloader_hold/events.h>

static int bootloader_warning_event_cb(const zmk_event_t *eh) {
    const struct zmk_bootloader_warning *ev = as_zmk_bootloader_warning(eh);
    if (ev == NULL || !ev->active) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    klor_central_widget_set_bootloader_pending();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(klor_bootloader_warn, bootloader_warning_event_cb);
ZMK_SUBSCRIPTION(klor_bootloader_warn, zmk_bootloader_warning);
```

(From `klor_central_widget.c`.) `zmk,bootloader-warn` only compiles its
listener on a central/unibody build (it calls `zmk_keymap_layer_active()`,
which doesn't exist on a split peripheral) — so this subscription only
ever needs to live in your central-side code. `ev->position`/`ev->layer`
are available if you're watching more than one position and want to react
differently per position; KLOR treats both the same and ignores them.

## 2. Decide: latch, or revert on early release?

This is the `latch` property on the devicetree node
(`dts/bindings/zmk,bootloader-warn.yaml`), and it changes what your
listener needs to handle:

- **`latch` set** (KLOR's choice): the event never fires with
  `active=false`. Once warned, treat it as permanent in your display code
  — no "un-warn" path exists, so don't write one. KLOR's reasoning: the
  frozen "BOOTLOADER" text is deliberately also a signal that *something*
  happened, even if the actual 3-second hold was released early and no
  reboot occurred — the only way back to normal is a real power cycle,
  which is an acceptable/intentional inconvenience for how rarely this
  should ever trigger.
- **`latch` unset** (default): an `active=false` event fires immediately
  on release if the warning had already shown, and your listener is
  expected to revert whatever it displayed. If you want this behavior,
  drop the `!ev->active` early-return above and handle both cases.

There's no wrong answer here — it's a product decision, not a technical
constraint. Pick based on whether you want the display to be a live status
indicator or a one-way "this almost happened" record.

## 3. The actual display change

KLOR's central widget keeps one `static bool bootloader_pending` latch,
set once by the callback above:

```c
static bool bootloader_pending;

void klor_central_widget_set_bootloader_pending(void) {
    bootloader_pending = true;
    submit_shadow_render();          /* triggers a re-render on the display work queue */
    klor_modifier_sync_set_bootloader_pending(); /* see part 4 below */
}
```

The render function checks that flag every time it runs and swaps which
LVGL objects are visible — hiding the normal layer-name/face-icon row and
showing a full-width centered label instead, rather than just changing
text in place, so it reads as an unmistakable full-row alert:

```c
if (bootloader_pending) {
    lv_obj_add_flag(widget->layer_name_badge.box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget->face_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget->bootloader_label, LV_OBJ_FLAG_HIDDEN);
} else {
    lv_obj_clear_flag(widget->layer_name_badge.box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget->face_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget->bootloader_label, LV_OBJ_FLAG_HIDDEN);
}
```

The label itself is created once, up front, hidden by default — not
allocated on demand when the warning fires. This avoids relying on LVGL
object creation happening correctly mid-runtime on a background work
queue; the object always exists, only its visibility changes:

```c
widget->bootloader_label = lv_label_create(widget->obj);
lv_obj_set_width(widget->bootloader_label, 128);
lv_obj_set_style_text_align(widget->bootloader_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
lv_obj_set_style_text_color(widget->bootloader_label, lv_color_black(), LV_PART_MAIN);
lv_obj_set_style_text_font(widget->bootloader_label, &pixel_operator_mono, LV_PART_MAIN);
lv_label_set_text(widget->bootloader_label, "BOOTLOADER");
lv_obj_align(widget->bootloader_label, LV_ALIGN_TOP_LEFT, 0, 43);
lv_obj_add_flag(widget->bootloader_label, LV_OBJ_FLAG_HIDDEN);
```

(`lv_color_black()` resolving to a visually *lit* label here is a
KLOR-specific quirk of that board's inverted-panel devicetree property,
not a general LVGL fact — use whatever your own panel's normal "lit
foreground" color is.)

None of this three-step pattern (bool flag → render-time visibility swap →
pre-created-but-hidden object) is specific to ZMK or this module; it's
just how to make an LVGL screen react to *any* rare state change safely.
The only ZMK/module-specific part is step 1.

## 4. If a peripheral half also has a display

This is the one part that's genuinely architecture-specific and won't
apply to every board. `zmk,bootloader-warn`'s listener only runs on the
central (see part 1) — a peripheral never receives the
`zmk_bootloader_warning` event directly, because ZMK doesn't resolve
layers/positions on a peripheral at all. If your peripheral has its own
independent display, you need *some* way to tell it "show the warning
too" — the same requirement as any other piece of per-peripheral display
state (battery, modifier indicators, connection status), and not something
this module can solve generically, since it depends entirely on whatever
split-communication mechanism your own project already has, or doesn't.

KLOR's situation: it already had a custom BLE GATT characteristic
(`klor_modifier_sync`) forwarding right-hand modifier state to the
peripheral for an unrelated reason, with a spare bit going unused in that
payload byte. Rather than build a second channel, it piggybacked:

```c
/* klor_modifier_sync_central.c */
static bool bootloader_pending;

void klor_modifier_sync_set_bootloader_pending(void) {
    bootloader_pending = true;
    send_mod_state(); /* forces an immediate GATT write, doesn't wait for the next mod/layer event */
}

/* ...inside the function that builds the outgoing payload byte: */
uint8_t payload = r_mods | (is_mac ? BIT(4) : 0) | (is_colemak ? BIT(5) : 0)
                 | (bootloader_pending ? BIT(6) : 0);
```

```c
/* klor_peripheral_widget.c, decoding the received byte */
if (payload & BIT(6)) {
    widget_state.bootloader_pending = true; /* one-way, matching `latch` -- never explicitly cleared */
}
```

The peripheral's own render function then does the *exact same*
hide-normal-content/show-label swap described in part 3, independently,
using its own local `bootloader_pending` flag — it never touches the
`zmk_bootloader_warning` event at all, since that event never reaches it.

If you don't already have a custom split-communication channel like this,
building one from scratch is out of scope for this guide — it means
implementing your own GATT service/characteristic pair (or reusing
whatever split RPC mechanism your board already has), which is real,
nontrivial C code independent of anything in this module. If your board
only has a display on the central half, none of this section applies to
you at all.
