# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_include_directories(include)
target_sources(app PRIVATE src/events.c)

# bootloader_warn.c calls zmk_keymap_layer_active(), which only exists on a
# central/unibody build (src/keymap.c isn't compiled for a split peripheral)
# -- and semantically this only ever makes sense there anyway, since a
# peripheral has no local layer state to gate on. Confirmed the hard way:
# klor_right (peripheral) failed with "undefined reference to
# zmk_keymap_layer_active" before this guard was added.
if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources(app PRIVATE src/bootloader_warn.c)
endif()
