# Ultimate CD-ROM

set(ULTIMATECDROM_SOURCES
    mitsumi.c
    ultimatecdrom.c
    ${PODULE_COMMON_DIR}/sound/sound_out_sdl2.c
    ${CDROM_SOURCE}
)

add_podule(ultimatecdrom
    SOURCES     ${ULTIMATECDROM_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/cdrom
        ${PODULE_COMMON_DIR}/sound
)
