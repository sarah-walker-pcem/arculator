# Design IT E200 Access+ podule

set(DESIGNIT_E200_SOURCES
    designit_e200.c
    ${PODULE_COMMON_DIR}/net/ne2000.c
    ${SLIRP_SOURCES}
)
if(WIN32)
    list(APPEND DESIGNIT_E200_SOURCES ${PODULE_COMMON_DIR}/net/net_pcap.c)
endif()

add_podule(designit_e200
    SOURCES     ${DESIGNIT_E200_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/net
        ${PODULE_COMMON_DIR}/net/slirp
)

if(WIN32)
    target_link_libraries(designit_e200 PRIVATE wsock32 iphlpapi kernel32)
endif()
