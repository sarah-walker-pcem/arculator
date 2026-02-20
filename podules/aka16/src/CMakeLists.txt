# AKA16

set(AKA16_SOURCES
    aka16.c
    ${PODULE_COMMON_DIR}/uart/scc2691.c
    ${MIDI_SOURCE}
)

add_podule(aka16
    SOURCES     ${AKA16_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/midi
        ${PODULE_COMMON_DIR}/uart
)

if(WIN32)
    target_link_libraries(aka16 PRIVATE winmm)
endif()
