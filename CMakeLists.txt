# CMakeLists.txt for Arculator
# Alternative build system — the canonical build uses GNU Autotools.

cmake_minimum_required(VERSION 3.14)
project(Arculator VERSION 2.2 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------- Options --------------------------------------------------------
option(ENABLE_DEBUG        "Build with debug flags"         OFF)
option(ENABLE_RELEASE_BUILD "Define RELEASE_BUILD"          OFF)
option(BUILD_GUI           "Build wxWidgets GUI binary"     ON)
option(BUILD_PODULES       "Build podule shared libraries"  ON)

# ---------- Compiler flags -------------------------------------------------
if(ENABLE_DEBUG)
    add_compile_options(-Wall -O0 -g)
    add_compile_definitions(_DEBUG DEBUG_LOG)
else()
    add_compile_options(-O3)
endif()

if(ENABLE_RELEASE_BUILD)
    add_compile_definitions(RELEASE_BUILD)
endif()

# ---------- Dependencies ---------------------------------------------------
find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.1)

# wxWidgets — only needed for the GUI binary.
if(BUILD_GUI)
    find_package(wxWidgets REQUIRED COMPONENTS core base xrc adv)
    include(${wxWidgets_USE_FILE})
endif()

# Platform libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(DL_LIBRARY dl REQUIRED)
    find_library(X11_LIBRARY X11 REQUIRED)
    if(BUILD_PODULES)
        pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
    endif()
endif()

# ---------- Podule helpers --------------------------------------------------
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(PoduleHelpers)

# ---------- Subdirectories -------------------------------------------------
add_subdirectory(src)

if(BUILD_PODULES)
    add_subdirectory(podules/aeh50/src)
    add_subdirectory(podules/aeh54/src)
    add_subdirectory(podules/aka05/src)
    add_subdirectory(podules/aka10/src)
    add_subdirectory(podules/aka12/src)
    add_subdirectory(podules/aka16/src)
    add_subdirectory(podules/aka31/src)
    add_subdirectory(podules/designit_e200/src)
    add_subdirectory(podules/lark/src)
    add_subdirectory(podules/midimax/src)
    add_subdirectory(podules/morley_uap/src)
    add_subdirectory(podules/oak_scsi/src)
    add_subdirectory(podules/pccard/src)
    add_subdirectory(podules/ultimatecdrom/src)
endif()
