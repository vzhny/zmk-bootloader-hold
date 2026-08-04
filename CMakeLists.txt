# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_include_directories(include)
target_sources(app PRIVATE src/events.c)
target_sources(app PRIVATE src/bootloader_warn.c)
