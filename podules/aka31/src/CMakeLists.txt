# AKA31

set(AKA31_SOURCES
    aka31.c
    d71071l.c
    wd33c93a.c
    ${SCSI_SOURCES}
    ${PODULE_COMMON_DIR}/sound/sound_out_sdl2.c
    ${CDROM_SOURCE}
)

add_podule(aka31
    SOURCES     ${AKA31_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/cdrom
        ${PODULE_COMMON_DIR}/sound
        ${PODULE_COMMON_DIR}/scsi
)
