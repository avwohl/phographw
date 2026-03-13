# Phograph for Windows - Visual Dataflow Programming

A Windows port of [Phograph](https://github.com/avwohl/phograph), a modern implementation of the [Prograph](https://en.wikipedia.org/wiki/Prograph) visual dataflow programming language.

Programs are built by connecting nodes with wires on a visual canvas rather than writing text. Data flows left-to-right through wires, and the system evaluates nodes as their inputs become available.

## Download for Windows

**[Download the latest Windows installer (Phograph.msix)](https://github.com/avwohl/phographw/releases/latest/download/Phograph.msix)**

See all releases: [GitHub Releases](https://github.com/avwohl/phographw/releases)

## Features

- **Visual graph editor** - drag nodes, connect wires, zoom/pan canvas
- **Dataflow evaluation** - token-based firing with automatic scheduling
- **14 data types** - integer, float, boolean, string, list, dict, object, enum, data, date, error, future, method-ref, nothing
- **~400 built-in primitives** - arithmetic, string, list, dict, type, date, JSON, I/O, scene graph, animation, method refs, observables
- **Classes and OOP** - inheritance, protocols, enums, data-determined dispatch
- **Multiple cases** - pattern matching with type/value/list/dict destructuring, case guards with type/value/wildcard
- **Control flow** - execution wires, evaluation nodes, loops, spreads, broadcasts, shift registers, error clusters
- **Async** - futures, channels, managed effects
- **Observable attributes** - reactive attribute system with actor classes
- **Front panel** - runtime UI for interactive programs
- **Scene graph** - shape hierarchy with CPU rasterizer, displayed via Direct2D
- **Debugger** - breakpoints, step/rollback, trace values on wires

## Prerequisites

You need a Windows PC with the following installed:

    Tool                              How to get it                                      Verify with
    ----                              ---------------                                    -----------
    Git                               https://git-scm.com/download/win                   git --version
    Visual Studio 2022 (C++ workload) Visual Studio Installer > "Desktop development     cl
                                      with C++"
    CMake 3.16+                       Included with Visual Studio, or cmake.org           cmake --version
    Windows SDK 10.0+                 Included with Visual Studio C++ workload            -

Visual Studio provides the MSVC C++17 compiler, the Windows SDK (Direct2D, DirectWrite, WinHTTP, Winsock, etc.), and CMake. If you install CMake via Visual Studio, run commands from a **Developer Command Prompt for VS 2022** or **Developer PowerShell** so that the compiler and SDK are on your PATH.

## Quick Start

Clone this repository and the shared core engine side by side:

    git clone https://github.com/avwohl/phograph.git
    git clone https://github.com/avwohl/phographw.git
    cd phographw

The Windows port references the portable C++ engine from `../phograph/phograph_core/src`, so both repositories must be siblings in the same parent directory.

### Build with CMake

    cmake -B build
    cmake --build build --config Release

This generates a Visual Studio solution under `build/` and compiles `Phograph.exe`.

### Open in Visual Studio

    cmake -B build
    start build\Phograph.sln

Select the **Phograph** target, set the configuration to **Release** or **Debug**, and press **F5** to build and run.

### Run the app

Once running:

1. **File > New** (Ctrl+N) creates a starter project that computes `(3 + 4) * 2 = 14`
2. Click **Run > Run** (Ctrl+R) to execute
3. **Ctrl+K** opens the fuzzy finder to add new nodes

## Architecture

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

## Building the MSIX Package

To build an MSIX installer for sideloading or Windows Store submission:

    packaging\build_msix.bat Release

This will:

1. Build the executable with CMake (Release config)
2. Stage the EXE, manifest, and store assets
3. Create `resources.pri` via MakePri.exe
4. Pack the MSIX via MakeAppx.exe
5. Sign with a self-signed development certificate

The output is `build\Phograph.msix`.

## Examples

Open the Example Browser with **Ctrl+Shift+E** to explore built-in examples:

- **Basics** -- arithmetic, string ops, control flow
- **Lists** -- map, filter, sort, list comprehensions
- **Classes** -- OOP with inheritance, instance generators, get/set
- **Graphics** -- scene graph shapes, canvas rendering
- **Patterns** -- loops, spreads, broadcasts, error handling

## Documentation

- [Learn Phograph](https://avwohl.github.io/phograph/) -- step-by-step tutorial covering dataflow basics through OOP and advanced patterns
- [IDE Guide](https://avwohl.github.io/phograph/guide.html) -- canvas navigation, keyboard shortcuts, debugger, and export
- [Language Reference](https://avwohl.github.io/phograph/reference.html) -- complete reference for all data types, primitives, and evaluation rules

## Contributing

Contributions are welcome.

1. Fork the repository
2. Create a feature branch (`git checkout -b my-feature`)
3. Make your changes -- follow existing code style
4. Build and verify:

       cmake -B build && cmake --build build --config Release

5. Open a pull request against `master`

## License

GNU General Public License v3.0 - see [LICENSE](LICENSE)

## Privacy

See [PRIVACY.md](PRIVACY.md)

## Author

Aaron Wohl

https://github.com/avwohl/phographw
