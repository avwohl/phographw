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

### Run the app

Once running:

1. **File > New** (Ctrl+N) creates a starter project that computes `(3 + 4) * 2 = 14`
2. Click **Run > Run** (Ctrl+R) to execute
3. **Ctrl+K** opens the fuzzy finder to add new nodes

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
- [Architecture](docs/architecture.md) -- source layout of this repo and the portable core, and the platform abstraction
- [Building](docs/building.md) -- building in Visual Studio and packaging the MSIX installer

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
