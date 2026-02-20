# AEH50 — Acorn AEH50 Ethernet II podule

set(AEH50_SOURCES
    aeh50.c
    ${PODULE_COMMON_DIR}/net/ne2000.c
    ${SLIRP_SOURCES}
)
if(WIN32)
    list(APPEND AEH50_SOURCES ${PODULE_COMMON_DIR}/net/net_pcap.c)
endif()

add_podule(aeh50
    SOURCES     ${AEH50_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/net
        ${PODULE_COMMON_DIR}/net/slirp
)

if(WIN32)
    target_link_libraries(aeh50 PRIVATE wsock32 iphlpapi kernel32)
endif()
