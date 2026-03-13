cmake_minimum_required(VERSION 3.16)
project(Phograph LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Core engine sources (from the shared phograph_core library)
# ---------------------------------------------------------------------------
set(CORE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../phograph/phograph_core/src")

set(CORE_SOURCES
    ${CORE_SRC_DIR}/pho_value.cc
    ${CORE_SRC_DIR}/pho_prim.cc
    ${CORE_SRC_DIR}/pho_prim_arith.cc
    ${CORE_SRC_DIR}/pho_prim_compare.cc
    ${CORE_SRC_DIR}/pho_prim_logic.cc
    ${CORE_SRC_DIR}/pho_prim_string.cc
    ${CORE_SRC_DIR}/pho_prim_list.cc
    ${CORE_SRC_DIR}/pho_prim_dict.cc
    ${CORE_SRC_DIR}/pho_prim_type.cc
    ${CORE_SRC_DIR}/pho_prim_data.cc
    ${CORE_SRC_DIR}/pho_prim_error.cc
    ${CORE_SRC_DIR}/pho_eval.cc
    ${CORE_SRC_DIR}/pho_serial.cc
    ${CORE_SRC_DIR}/pho_bridge.cc
    ${CORE_SRC_DIR}/pho_draw.cc
    ${CORE_SRC_DIR}/pho_prim_canvas.cc
    ${CORE_SRC_DIR}/pho_prim_scene.cc
    ${CORE_SRC_DIR}/pho_prim_draw.cc
    ${CORE_SRC_DIR}/pho_prim_input.cc
    ${CORE_SRC_DIR}/pho_prim_anim.cc
    ${CORE_SRC_DIR}/pho_prim_io.cc
    ${CORE_SRC_DIR}/pho_thread.cc
    ${CORE_SRC_DIR}/pho_debug.cc
    ${CORE_SRC_DIR}/pho_prim_future.cc
    ${CORE_SRC_DIR}/pho_prim_channel.cc
    ${CORE_SRC_DIR}/pho_prim_effect.cc
    ${CORE_SRC_DIR}/pho_codegen.cc
    ${CORE_SRC_DIR}/pho_prim_math.cc
    # pho_prim_fileio.cc excluded – replaced by src/pho_prim_fileio_win.cc
    # pho_prim_socket.cc excluded – replaced by src/pho_prim_socket_win.cc
    ${CORE_SRC_DIR}/pho_prim_sound.cc
    ${CORE_SRC_DIR}/pho_prim_midi.cc
    # pho_prim_locale.cc excluded – replaced by src/pho_prim_locale_win.cc
    ${CORE_SRC_DIR}/pho_prim_crypto.cc
    ${CORE_SRC_DIR}/pho_prim_image.cc
    ${CORE_SRC_DIR}/pho_prim_bitmap.cc
    ${CORE_SRC_DIR}/pho_prim_net.cc
    # pho_prim_date.cc excluded – replaced by src/pho_prim_date_win.cc
    ${CORE_SRC_DIR}/pho_prim_methodref.cc
)
# NOTE: pho_platform_stub.cc is intentionally excluded (replaced by
#       pho_platform_windows.cc below).
# NOTE: plugins/SoundPlugin.cpp and plugins/MIDIPlugin.cpp are excluded
#       (replaced by plugins_windows.cc below).

# ---------------------------------------------------------------------------
# Windows application sources
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cc
    src/app.cc
    src/main_window.cc
    src/graph_canvas.cc
    src/dialogs.cc
    src/examples.cc
    src/pho_platform_windows.cc
    src/plugins_windows.cc
    src/pho_prim_fileio_win.cc
    src/pho_prim_socket_win.cc
    src/pho_prim_locale_win.cc
    src/pho_prim_date_win.cc
    src/resource.rc
)

# ---------------------------------------------------------------------------
# Executable – WIN32 flag suppresses the console window
# ---------------------------------------------------------------------------
add_executable(Phograph WIN32
    ${CORE_SOURCES}
    ${APP_SOURCES}
)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(Phograph PRIVATE
    ${CORE_SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Preprocessor definitions
# ---------------------------------------------------------------------------
target_compile_definitions(Phograph PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    _USE_MATH_DEFINES
    NOMINMAX
)

# ---------------------------------------------------------------------------
# Windows libraries
# ---------------------------------------------------------------------------
target_link_libraries(Phograph PRIVATE
    d2d1
    dwrite
    windowscodecs
    winhttp
    comctl32
    shlwapi
    user32
    gdi32
    shell32
    ole32
    comdlg32
    advapi32
    bcrypt
    winmm
    ws2_32
)

# ---------------------------------------------------------------------------
# Linker: set Windows subsystem explicitly
# ---------------------------------------------------------------------------
if(MSVC)
    target_link_options(Phograph PRIVATE /SUBSYSTEM:WINDOWS)
    # Force-include compatibility header for core sources missing MSVC headers
    target_compile_options(Phograph PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/src/win_compat.h")
endif()
