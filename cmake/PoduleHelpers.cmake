# cmake/PoduleHelpers.cmake — helpers for building Arculator podule modules.

set(PODULE_COMMON_DIR "${CMAKE_SOURCE_DIR}/podules/common")

# SLIRP networking sources shared by aeh50, aeh54, designit_e200
set(SLIRP_SOURCES
    ${PODULE_COMMON_DIR}/net/net.c
    ${PODULE_COMMON_DIR}/net/net_slirp.c
    ${PODULE_COMMON_DIR}/net/slirp/bootp.c
    ${PODULE_COMMON_DIR}/net/slirp/cksum.c
    ${PODULE_COMMON_DIR}/net/slirp/debug.c
    ${PODULE_COMMON_DIR}/net/slirp/if.c
    ${PODULE_COMMON_DIR}/net/slirp/ip_icmp.c
    ${PODULE_COMMON_DIR}/net/slirp/ip_input.c
    ${PODULE_COMMON_DIR}/net/slirp/ip_output.c
    ${PODULE_COMMON_DIR}/net/slirp/mbuf.c
    ${PODULE_COMMON_DIR}/net/slirp/misc.c
    ${PODULE_COMMON_DIR}/net/slirp/queue.c
    ${PODULE_COMMON_DIR}/net/slirp/sbuf.c
    ${PODULE_COMMON_DIR}/net/slirp/slirp.c
    ${PODULE_COMMON_DIR}/net/slirp/socket.c
    ${PODULE_COMMON_DIR}/net/slirp/tcp_input.c
    ${PODULE_COMMON_DIR}/net/slirp/tcp_output.c
    ${PODULE_COMMON_DIR}/net/slirp/tcp_subr.c
    ${PODULE_COMMON_DIR}/net/slirp/tcp_timer.c
    ${PODULE_COMMON_DIR}/net/slirp/tftp.c
    ${PODULE_COMMON_DIR}/net/slirp/udp.c
)

# Common SCSI sources shared by aka31, oak_scsi
set(SCSI_SOURCES
    ${PODULE_COMMON_DIR}/scsi/hdd_file.c
    ${PODULE_COMMON_DIR}/scsi/scsi.c
    ${PODULE_COMMON_DIR}/scsi/scsi_cd.c
    ${PODULE_COMMON_DIR}/scsi/scsi_config.c
    ${PODULE_COMMON_DIR}/scsi/scsi_hd.c
)

# Platform-specific MIDI source
if(WIN32)
    set(MIDI_SOURCE ${PODULE_COMMON_DIR}/midi/midi_win.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MIDI_SOURCE ${PODULE_COMMON_DIR}/midi/midi_alsa.c)
elseif(APPLE)
    set(MIDI_SOURCE ${PODULE_COMMON_DIR}/midi/midi_null.c)
endif()

# Platform-specific CD-ROM source
if(WIN32)
    set(CDROM_SOURCE ${PODULE_COMMON_DIR}/cdrom/cdrom-windows-ioctl.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CDROM_SOURCE ${PODULE_COMMON_DIR}/cdrom/cdrom-linux-ioctl.c)
elseif(APPLE)
    set(CDROM_SOURCE ${PODULE_COMMON_DIR}/cdrom/cdrom-osx-ioctl.c)
endif()

# add_podule(<name> SOURCES src1 src2 ...
#            [INCLUDE_DIRS dir1 dir2 ...]
#            [LIBRARIES lib1 lib2 ...])
#
# Creates a MODULE library (shared, runtime-loaded) named <name>,
# copies the resulting .so/.dll/.dylib next to the podule source dir.
function(add_podule NAME)
    cmake_parse_arguments(POD "" "" "SOURCES;INCLUDE_DIRS;LIBRARIES" ${ARGN})

    add_library(${NAME} MODULE ${POD_SOURCES})

    # Every podule needs the main src/ headers
    target_include_directories(${NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${POD_INCLUDE_DIRS}
    )

    target_link_libraries(${NAME} PRIVATE
        PkgConfig::SDL2
        ${POD_LIBRARIES}
    )

    # Strip the "lib" prefix so the output is e.g. "aeh50.so" not "libaeh50.so"
    set_target_properties(${NAME} PROPERTIES PREFIX "")

    # Copy result next to the podule src/ directory (one level up),
    # matching autotools behaviour which puts .so alongside the podule data.
    add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${NAME}>
                ${CMAKE_CURRENT_SOURCE_DIR}/../$<TARGET_FILE_NAME:${NAME}>
        COMMENT "Copying ${NAME} podule to source tree"
    )
endfunction()
