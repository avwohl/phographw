# Phograph for Windows - Visual Dataflow Programming

The Windows port of [Phograph](https://github.com/avwohl/phograph), a modern implementation of the [Prograph](https://en.wikipedia.org/wiki/Prograph) visual dataflow programming language.

Programs are built by connecting nodes with wires on a visual canvas rather than writing text. Data flows left-to-right through wires, and the system evaluates nodes as their inputs become available.

## Features

- **Visual graph editor** - drag nodes, connect wires, zoom/pan canvas
- **Dataflow evaluation** - token-based firing with automatic scheduling
- **14 data types** - integer, float, boolean, string, list, dict, object, enum, data, date, error, future, method-ref, nothing
- **~400 built-in primitives** - arithmetic, string, list, dict, type, date, JSON, I/O, and more
- **Classes and OOP** - inheritance, protocols, enums, data-determined dispatch
- **Multiple cases** - pattern matching with type/value/list/dict destructuring, case guards
- **Control flow** - execution wires, evaluation nodes, loops, spreads, broadcasts, shift registers, error clusters
- **Async** - futures, channels, managed effects
- **Debugger** - breakpoints, step/rollback, trace values on wires
- **Compiler** - graph-to-source code generation

## Prerequisites

    Tool          How to get it                      Verify with
    ----          ---------------                    -----------
    Git           git-scm.com                        git --version
    Visual Studio Visual Studio 2022 (C++ workload)  cl /?
    CMake 3.16+   Included with VS, or cmake.org     cmake --version

Visual Studio provides the MSVC C++17 compiler and the Windows SDK.

## Quick Start

Clone the repository:

    git clone https://github.com/avwohl/phographw.git
    cd phographw

### Option A: Build with CMake + Visual Studio

    cmake -B build
    cmake --open build

This generates a Visual Studio solution in `build/`. Open it, select the **Phograph** target, and press **F5** to build and run.

Or build from the command line:

    cmake -B build
    cmake --build build --config Debug
    build\Debug\Phograph.exe

### Option B: Command-line build

    cmake -B build
    cmake --build build --config Release
    build\Release\Phograph.exe

Once running:

1. **File > New** (Ctrl+N) creates a starter project that computes `(3 + 4) * 2 = 14`
2. Click **Run** in the toolbar to execute
3. **Ctrl+K** opens the fuzzy finder to add new nodes

## Architecture

The project shares the portable C++ engine from the macOS version, with a native Win32 UI layer:

    phographw/                   Windows IDE + app
      src/
        main.cc                  WinMain entry point
        app.{h,cc}               Application logic (wraps C++ engine)
        main_window.{h,cc}       Win32 main window, menus, toolbar, layout
        graph_canvas.{h,cc}      Direct2D graph rendering canvas
        dialogs.{h,cc}           Fuzzy finder and about dialogs
        pho_platform_windows.cc  Platform abstraction (Windows impl)
        pho_prim_date_win.cc     Date/time primitives (Windows)
        pho_prim_fileio_win.cc   File I/O primitives (Windows)
        pho_prim_locale_win.cc   Locale primitives (Windows)
        pho_prim_socket_win.cc   Socket primitives (Winsock)
        plugins_windows.cc       Sound/MIDI plugin stubs
        resource.{h,rc}          Menus, accelerators, resources
        win_compat.h             MSVC compatibility shims

    ../phograph/phograph_core/   Portable C++ engine (shared with macOS)
      src/
        pho_value.{h,cc}         Tagged union for 13 types
        pho_eval.{h,cc}          Dataflow evaluator/scheduler
        pho_serial.{h,cc}        JSON serialization/deserialization
        pho_bridge.{h,cc}        C API for interop
        pho_prim*.cc             Primitive implementations (~25 files)
        pho_draw.{h,cc}          CPU rasterizer
        pho_codegen.{h,cc}       Graph-to-source compiler
        pho_debug.{h,cc}         Debugger/trace
        pho_thread.{h,cc}        Run loop, timers, event queue
        pho_platform.h           Platform abstraction interface

The C++ core has zero platform `#include`s. All I/O goes through `pho_platform.h`, which has per-platform implementations.

## Contributing

Contributions are welcome.

1. Fork the repository
2. Create a feature branch (`git checkout -b my-feature`)
3. Make your changes -- follow existing code style
4. Build and test:

       cmake -B build && cmake --build build --config Debug

5. Open a pull request against `master`

## Related Projects

- **[Phograph](https://github.com/avwohl/phograph)** - The macOS/iOS version of Phograph (SwiftUI + Metal). Shares the same portable C++ engine.

## License

MIT License - see [LICENSE](LICENSE)

## Privacy

See [PRIVACY.md](PRIVACY.md)

## Author

Aaron Wohl

https://github.com/avwohl/phographw
