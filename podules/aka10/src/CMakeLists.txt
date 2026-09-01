# AKA10

set(AKA10_SOURCES
    aka10.c
    ${PODULE_COMMON_DIR}/uart/6850.c
    ${PODULE_COMMON_DIR}/joystick/joystick_sdl2.c
    ${PODULE_COMMON_DIR}/adc/d7002c.c
    ${PODULE_COMMON_DIR}/misc/6522.c
    ${MIDI_SOURCE}
)

add_podule(aka10
    SOURCES     ${AKA10_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/adc
        ${PODULE_COMMON_DIR}/joystick
        ${PODULE_COMMON_DIR}/midi
        ${PODULE_COMMON_DIR}/misc
        ${PODULE_COMMON_DIR}/uart
)

if(WIN32)
    target_link_libraries(aka10 PRIVATE winmm)
endif()
