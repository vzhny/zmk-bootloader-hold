/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bootloader_warn

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/init.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#include <bootloader_hold/events.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct bootloader_warn_slot {
    uint32_t position;
    uint8_t layer;
    uint32_t warn_after_ms;
    bool latch;
    bool held;
    bool warned;
    struct k_work_delayable work;
};

static void bootloader_warn_fire(struct k_work *work);

#define SLOT_INIT(inst)                                                                            \
    {                                                                                              \
        .position = DT_INST_PROP(inst, position),                                                 \
        .layer = DT_INST_PROP(inst, layer),                                                        \
        .warn_after_ms = DT_INST_PROP(inst, warn_after_ms),                                        \
        .latch = DT_INST_PROP(inst, latch),                                                        \
    },

static struct bootloader_warn_slot slots[] = {DT_INST_FOREACH_STATUS_OKAY(SLOT_INIT)};
#define SLOT_COUNT ARRAY_SIZE(slots)

static void bootloader_warn_fire(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct bootloader_warn_slot *slot = CONTAINER_OF(dwork, struct bootloader_warn_slot, work);

    if (!slot->held) {
        return; /* released before warn_after_ms -- nothing to announce */
    }
    slot->warned = true;
    raise_zmk_bootloader_warning((struct zmk_bootloader_warning){
        .position = slot->position,
        .layer = slot->layer,
        .active = true,
        .timestamp = k_uptime_get(),
    });
}

static int position_event_cb(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (size_t i = 0; i < SLOT_COUNT; i++) {
        struct bootloader_warn_slot *slot = &slots[i];
        if (slot->position != ev->position) {
            continue;
        }

        if (ev->state) {
            /* Only arm if the configured layer is actually active -- this
             * position is an ordinary key on every other layer, and this
             * must never fire from normal typing. */
            if (zmk_keymap_layer_active(slot->layer)) {
                slot->held = true;
                slot->warned = false;
                k_work_schedule(&slot->work, K_MSEC(slot->warn_after_ms));
            }
        } else {
            slot->held = false;
            k_work_cancel_delayable(&slot->work);
            if (slot->warned && !slot->latch) {
                slot->warned = false;
                raise_zmk_bootloader_warning((struct zmk_bootloader_warning){
                    .position = slot->position,
                    .layer = slot->layer,
                    .active = false,
                    .timestamp = k_uptime_get(),
                });
            }
        }
        /* Deliberately no `break` -- in the unusual case where more than
         * one instance watches the same position (e.g. two different
         * layers), all matching slots should independently track it. */
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bootloader_warn, position_event_cb);
ZMK_SUBSCRIPTION(bootloader_warn, zmk_position_state_changed);

static int bootloader_warn_init(void) {
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        k_work_init_delayable(&slots[i].work, bootloader_warn_fire);
    }
    return 0;
}

SYS_INIT(bootloader_warn_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
