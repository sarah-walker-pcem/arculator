# AEH54 — Acorn AEH54 Ethernet III podule

set(AEH54_SOURCES
    aeh54.c
    seeq8005.c
    ${SLIRP_SOURCES}
)
if(WIN32)
    list(APPEND AEH54_SOURCES ${PODULE_COMMON_DIR}/net/net_pcap.c)
endif()

add_podule(aeh54
    SOURCES     ${AEH54_SOURCES}
    INCLUDE_DIRS
        ${PODULE_COMMON_DIR}/net
        ${PODULE_COMMON_DIR}/net/slirp
)

if(WIN32)
    target_link_libraries(aeh54 PRIVATE wsock32 iphlpapi kernel32)
endif()
