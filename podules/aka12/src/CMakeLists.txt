# AKA12

set(AKA12_SOURCES
    aka12.c
    ${PODULE_COMMON_DIR}/misc/6522.c
    ${PODULE_COMMON_DIR}/uart/scc2691.c
    ${MIDI_SOURCE}
)

add_podule(aka12
    SOURCES     ${AKA12_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/midi
        ${PODULE_COMMON_DIR}/misc
        ${PODULE_COMMON_DIR}/uart
)

if(WIN32)
    target_link_libraries(aka12 PRIVATE winmm)
endif()
