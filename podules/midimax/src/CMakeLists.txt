# MIDI Max

set(MIDIMAX_SOURCES
    midimax.c
    ${PODULE_COMMON_DIR}/uart/16550.c
    ${MIDI_SOURCE}
)

add_podule(midimax
    SOURCES     ${MIDIMAX_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/midi
        ${PODULE_COMMON_DIR}/uart
)

if(WIN32)
    target_link_libraries(midimax PRIVATE winmm)
endif()
