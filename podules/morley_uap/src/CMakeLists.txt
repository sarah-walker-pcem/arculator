# Morley A3000 User and Analogue Port

set(MORLEY_UAP_SOURCES
    morley_uap.c
    ${PODULE_COMMON_DIR}/joystick/joystick_sdl2.c
    ${PODULE_COMMON_DIR}/adc/d7002c.c
    ${PODULE_COMMON_DIR}/misc/6522.c
)

add_podule(morley_uap
    SOURCES     ${MORLEY_UAP_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/adc
        ${PODULE_COMMON_DIR}/joystick
        ${PODULE_COMMON_DIR}/misc
)
