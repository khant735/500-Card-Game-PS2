    .section .rodata

    /* Permanently resident artwork, stored compressed in the ELF / EE RAM. */
    .balign 128
    .global asset_felt_z
asset_felt_z:
    .incbin "assets/felt_256x128_t8.z"
    .global asset_felt_z_end
asset_felt_z_end:

    .balign 128
    .global asset_felt_pal
asset_felt_pal:
    .incbin "assets/felt_256x128_t8.pal"

    .balign 128
    .global asset_wood_z
asset_wood_z:
    .incbin "assets/wood_128x32_t8.z"
    .global asset_wood_z_end
asset_wood_z_end:

    .balign 128
    .global asset_wood_pal
asset_wood_pal:
    .incbin "assets/wood_128x32_t8.pal"

    .balign 128
    .global asset_leather_z
asset_leather_z:
    .incbin "assets/leather_128x32_t8.z"
    .global asset_leather_z_end
asset_leather_z_end:

    .balign 128
    .global asset_leather_pal
asset_leather_pal:
    .incbin "assets/leather_128x32_t8.pal"

    .balign 128
    .global asset_cardgrain_z
asset_cardgrain_z:
    .incbin "assets/cardgrain_32x32_ct16.z"
    .global asset_cardgrain_z_end
asset_cardgrain_z_end:

    .balign 128
    .global asset_cardback_z
asset_cardback_z:
    .incbin "assets/cardback_501B_exact.z"
    .global asset_cardback_z_end
asset_cardback_z_end:

    .balign 128
    .global asset_chips_z
asset_chips_z:
    .incbin "assets/chips_64x32_plain_ct16.z"
    .global asset_chips_z_end
asset_chips_z_end:

    /* Streamed 16-bit special-card images. Courts 0..11, Jokers 12..13. */
    .balign 128
    .global asset_special_00_z
asset_special_00_z:
    .incbin "assets/court_00_128x224_ct16.z"
    .global asset_special_00_z_end
asset_special_00_z_end:
    .balign 128
    .global asset_special_01_z
asset_special_01_z:
    .incbin "assets/court_01_128x224_ct16.z"
    .global asset_special_01_z_end
asset_special_01_z_end:
    .balign 128
    .global asset_special_02_z
asset_special_02_z:
    .incbin "assets/court_02_128x224_ct16.z"
    .global asset_special_02_z_end
asset_special_02_z_end:
    .balign 128
    .global asset_special_03_z
asset_special_03_z:
    .incbin "assets/court_03_128x224_ct16.z"
    .global asset_special_03_z_end
asset_special_03_z_end:
    .balign 128
    .global asset_special_04_z
asset_special_04_z:
    .incbin "assets/court_04_128x224_ct16.z"
    .global asset_special_04_z_end
asset_special_04_z_end:
    .balign 128
    .global asset_special_05_z
asset_special_05_z:
    .incbin "assets/court_05_128x224_ct16.z"
    .global asset_special_05_z_end
asset_special_05_z_end:
    .balign 128
    .global asset_special_06_z
asset_special_06_z:
    .incbin "assets/court_06_128x224_ct16.z"
    .global asset_special_06_z_end
asset_special_06_z_end:
    .balign 128
    .global asset_special_07_z
asset_special_07_z:
    .incbin "assets/court_07_128x224_ct16.z"
    .global asset_special_07_z_end
asset_special_07_z_end:
    .balign 128
    .global asset_special_08_z
asset_special_08_z:
    .incbin "assets/court_08_128x224_ct16.z"
    .global asset_special_08_z_end
asset_special_08_z_end:
    .balign 128
    .global asset_special_09_z
asset_special_09_z:
    .incbin "assets/court_09_128x224_ct16.z"
    .global asset_special_09_z_end
asset_special_09_z_end:
    .balign 128
    .global asset_special_10_z
asset_special_10_z:
    .incbin "assets/court_10_128x224_ct16.z"
    .global asset_special_10_z_end
asset_special_10_z_end:
    .balign 128
    .global asset_special_11_z
asset_special_11_z:
    .incbin "assets/court_11_128x224_ct16.z"
    .global asset_special_11_z_end
asset_special_11_z_end:
    .balign 128
    .global asset_special_12_z
asset_special_12_z:
    .incbin "assets/special_12_128x256.z"
    .global asset_special_12_z_end
asset_special_12_z_end:
    .balign 128
    .global asset_special_13_z
asset_special_13_z:
    .incbin "assets/special_13_128x256.z"
    .global asset_special_13_z_end
asset_special_13_z_end: