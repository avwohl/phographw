# Architecture

The project follows a portable C++ core with thin platform bridge pattern:

    phographw/                   Windows IDE + app (this repo)
      src/
        main.cc                  WinMain entry point
        app.h/cc                 Application state and engine wrapper
        main_window.h/cc         Main window, menus, accelerators
        graph_canvas.h/cc        Direct2D canvas rendering and interaction
        dialogs.h/cc             Dialog boxes (add node, inspector)
        examples.h/cc            Example browser (downloads from GitHub)
        pho_platform_windows.cc  Platform abstraction (file I/O, HTTP, threading)
        plugins_windows.cc       Audio/MIDI plugin stubs (Windows)
        pho_prim_fileio_win.cc   Windows file I/O primitives
        pho_prim_socket_win.cc   Windows socket primitives
        pho_prim_locale_win.cc   Windows locale/time primitives
        pho_prim_date_win.cc     Windows date/time primitives
        resource.rc              Menus, accelerators, version info
        resource.h               Resource IDs
        version.h                Version constants
        win_compat.h             MSVC compatibility shims for core sources
      packaging/
        AppxManifest.xml         MSIX manifest for Windows Store
        build_msix.bat           Automated MSIX packaging script
      store_assets/              Windows Store tile/logo images

    ../phograph/phograph_core/   Portable C++ engine (separate repo)
      src/
        pho_value.{h,cc}         Tagged union for 13 types
        pho_graph.{h,cc}         Graph model: Node, Wire, Method, Case, Class
        pho_eval.{h,cc}          Dataflow evaluator/scheduler
        pho_serial.{h,cc}        JSON serialization/deserialization
        pho_bridge.{h,cc}        C API for platform interop
        pho_prim*.cc             Primitive implementations (~25 files)
        pho_scene.{h,cc}         Scene graph
        pho_draw.{h,cc}          CPU rasterizer
        pho_codegen.{h,cc}       Graph-to-Swift compiler
        pho_debug.{h,cc}         Debugger/trace
        pho_thread.{h,cc}        Run loop, timers, event queue
        pho_platform.h           Platform abstraction (no implementation)
      tests/                     C++ test suite (13 executables)

The C++ core has zero platform `#include`s. All I/O goes through `pho_platform.h`, which has per-platform implementations (Windows: `src/pho_platform_windows.cc`, macOS: in the main repo's `Bridge/pho_platform_apple.mm`).
