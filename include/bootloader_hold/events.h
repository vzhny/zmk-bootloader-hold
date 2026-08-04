/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

/* Raised by a "zmk,bootloader-warn" devicetree node (src/bootloader_warn.c)
 * partway through a &bootloader_hold hold. `active` distinguishes the two
 * cases a listener needs to handle -- see that node's `latch` property doc
 * (dts/bindings/zmk,bootloader-warn.yaml) for exactly when active=false
 * does or doesn't fire. Subscribe in your own project with a plain
 * ZMK_LISTENER/ZMK_SUBSCRIPTION against zmk_bootloader_warning -- nothing
 * special needed to consume an event from a different module than the one
 * that raises it. */
struct zmk_bootloader_warning {
    uint32_t position;
    uint8_t layer;
    bool active;
    int64_t timestamp;
};

ZMK_EVENT_DECLARE(zmk_bootloader_warning);
