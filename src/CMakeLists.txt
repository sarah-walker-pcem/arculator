# CMakeLists.txt for the main Arculator executable(s)

# ---------- Core emulation sources (no wx dependency) -----------------
set(ARCULATOR_CORE_SOURCES
    82c711.c
    82c711_fdc.c
    arm.c
    bmu.c
    cmos.c
    colourcard.c
    config.c
    cp15.c
    ddnoise.c
    debugger.c
    debugger_swis.c
    disc.c
    disc_adf.c
    disc_apd.c
    disc_fdi.c
    disc_hfe.c
    disc_jfd.c
    disc_mfm_common.c
    disc_scp.c
    ds2401.c
    eterna.c
    fdi2raw.c
    fpa.c
    g16.c
    g332.c
    hostfs.c
    ide.c
    ide_a3in.c
    ide_config.c
    ide_idea.c
    ide_riscdev.c
    ide_zidefs.c
    ide_zidefs_a3k.c
    input_sdl2.c
    ioc.c
    ioeb.c
    joystick.c
    keyboard.c
    lc.c
    main.c
    mem.c
    memc.c
    podules.c
    printer.c
    riscdev_hdfc.c
    romload.c
    sound.c
    sound_sdl2.c
    st506.c
    st506_akd52.c
    timer.c
    vidc.c
    video_sdl2.c
    wd1770.c
    wx-sdl2-joystick.c
)

# ---------- Platform sources ------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND ARCULATOR_PLATFORM_SOURCES hostfs-unix.c podules-linux.c)
elseif(APPLE)
    list(APPEND ARCULATOR_PLATFORM_SOURCES hostfs-unix.c podules-macosx.c)
elseif(WIN32)
    list(APPEND ARCULATOR_PLATFORM_SOURCES hostfs-win.c podules-win.c)
endif()

# ---------- wxWidgets GUI sources -------------------------------------
set(ARCULATOR_WX_SOURCES
    wx-app.cc
    wx-config.cc
    wx-config_sel.cc
    wx-hd_conf.cc
    wx-console.cc
    wx-hd_new.cc
    wx-joystick-config.cc
    wx-main.cc
    wx-podule-config.cc
    wx-resources.cc
)

# SDL event-loop source for the GUI (has a wx/defs.h include)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND ARCULATOR_WX_SOURCES wx-sdl2.c)
elseif(APPLE)
    list(APPEND ARCULATOR_WX_SOURCES wx-sdl2.c)
elseif(WIN32)
    list(APPEND ARCULATOR_WX_SOURCES wx-win32.c)
endif()

# ======================================================================
# GUI target (requires wxWidgets)
# ======================================================================
if(BUILD_GUI)
    # Generate wx-resources.cc from XRC
    find_program(WXRC_EXECUTABLE wxrc)
    if(WXRC_EXECUTABLE)
        add_custom_command(
            OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/wx-resources.cc
            COMMAND ${WXRC_EXECUTABLE} -c
                    ${CMAKE_CURRENT_SOURCE_DIR}/arculator.xrc
                    -o ${CMAKE_CURRENT_SOURCE_DIR}/wx-resources.cc
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/arculator.xrc
            COMMENT "Generating wx-resources.cc from arculator.xrc"
        )
    else()
        if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/wx-resources.cc)
            message(FATAL_ERROR
                "wxrc not found and wx-resources.cc does not exist.\n"
                "Install the wxWidgets tools package (e.g. wx3.2-headers or wx-common).")
        endif()
    endif()

    add_executable(arculator
        ${ARCULATOR_CORE_SOURCES}
        ${ARCULATOR_PLATFORM_SOURCES}
        ${ARCULATOR_WX_SOURCES}
    )

    target_include_directories(arculator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # wxWidgets: strip -fpermissive for C sources (match autotools behaviour)
    target_compile_options(arculator PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${wxWidgets_CXX_FLAGS}>
    )

    target_link_libraries(arculator PRIVATE
        ${wxWidgets_LIBRARIES}
        PkgConfig::SDL2
        ZLIB::ZLIB
        Threads::Threads
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(arculator PRIVATE ${DL_LIBRARY} ${X11_LIBRARY})
    elseif(APPLE)
        target_link_libraries(arculator PRIVATE ${X11_LIBRARY})
    endif()

    add_custom_command(TARGET arculator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:arculator>
                ${CMAKE_SOURCE_DIR}/arculator
        COMMENT "Copying arculator to project root"
    )
endif()

# ======================================================================
# CLI target (no wxWidgets — SDL2 only)
# ======================================================================
add_executable(arculator-cli
    ${ARCULATOR_CORE_SOURCES}
    ${ARCULATOR_PLATFORM_SOURCES}
    nogui.c
)

target_include_directories(arculator-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(arculator-cli PRIVATE
    PkgConfig::SDL2
    ZLIB::ZLIB
    Threads::Threads
    m
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(arculator-cli PRIVATE ${DL_LIBRARY} ${X11_LIBRARY})
elseif(APPLE)
    target_link_libraries(arculator-cli PRIVATE ${X11_LIBRARY})
endif()

add_custom_command(TARGET arculator-cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:arculator-cli>
            ${CMAKE_SOURCE_DIR}/arculator-cli
    COMMENT "Copying arculator-cli to project root"
)
