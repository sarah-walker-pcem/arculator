# Aleph One 386/486PC Expansion Card

set(PCCARD_SOURCES
    pccard_podule.c
    pcem/808x.c
    pcem/386.c
    pcem/386_common.c
    pcem/386_dynarec.c
    pcem/cpu.c
    pcem/cpu_tables.c
    pcem/diva.c
    pcem/dma.c
    pcem/io.c
    pcem/lpt.c
    pcem/mem.c
    pcem/pc.c
    pcem/pic.c
    pcem/pit.c
    pcem/scamp.c
    pcem/serial.c
    pcem/timer.c
    pcem/x86seg.c
    pcem/x87.c
    pcem/x87_timings.c
    libco/libco.c
)

add_podule(pccard
    SOURCES     ${PCCARD_SOURCES}
)
