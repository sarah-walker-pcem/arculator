# Lark — audio podule

set(LARK_SOURCES
    lark.c
    ad1848.c
    am7202a.c
    ${PODULE_COMMON_DIR}/uart/16550.c
    ${MIDI_SOURCE}
    ${PODULE_COMMON_DIR}/sound/sound_out_sdl2.c
)
if(WIN32)
    list(APPEND LARK_SOURCES ${PODULE_COMMON_DIR}/sound/sound_wavein.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND LARK_SOURCES ${PODULE_COMMON_DIR}/sound/sound_alsain.c)
elseif(APPLE)
    list(APPEND LARK_SOURCES ${PODULE_COMMON_DIR}/sound/sound_in_null.c)
endif()

add_podule(lark
    SOURCES     ${LARK_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/midi
        ${PODULE_COMMON_DIR}/sound
        ${PODULE_COMMON_DIR}/uart
)

if(WIN32)
    target_link_libraries(lark PRIVATE winmm)
endif()
