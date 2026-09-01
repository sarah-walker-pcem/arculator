# Oak SCSI

set(OAK_SCSI_SOURCES
    oak_scsi.c
    ncr5380.c
    ${SCSI_SOURCES}
    ${PODULE_COMMON_DIR}/sound/sound_out_sdl2.c
    ${PODULE_COMMON_DIR}/eeprom/93c06.c
    ${CDROM_SOURCE}
)

add_podule(oak_scsi
    SOURCES     ${OAK_SCSI_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/cdrom
        ${PODULE_COMMON_DIR}/eeprom
        ${PODULE_COMMON_DIR}/sound
        ${PODULE_COMMON_DIR}/scsi
)
