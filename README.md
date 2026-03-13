# Phograph - Visual Dataflow Programming

A modern implementation of the [Prograph](https://en.wikipedia.org/wiki/Prograph) visual dataflow programming language for macOS.

Programs are built by connecting nodes with wires on a visual canvas rather than writing text. Data flows left-to-right through wires, and the system evaluates nodes as their inputs become available.

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
- **Scene graph** - shape hierarchy with CPU rasterizer, displayed via Metal
- **Compiler** - graph-to-Swift source code generation for standalone binaries
- **Debugger** - breakpoints, step/rollback, trace values on wires
- **10 plugin libraries** - math, crypto, image, sound, MIDI, networking, file I/O, and more (135 additional primitives)

## Prerequisites

You need a Mac with the following installed:

    Tool          How to get it                      Verify with
    ----          ---------------                    -----------
    Git           Xcode Command Line Tools (below)   git --version
    Xcode 15+     Mac App Store                      xcodebuild -version
    CMake 3.16+   brew install cmake                 cmake --version

Xcode installs the Apple Clang C++17 compiler, the macOS SDKs (CoreAudio, CoreMIDI, CoreGraphics, ImageIO, Security, etc.), and the Swift toolchain. If you only want to run the C++ engine tests and do not need the full IDE, you can use the Xcode Command Line Tools alone (without the full Xcode app):

    xcode-select --install       # installs git, clang, make, etc.
    brew install cmake           # CMake is not included with Xcode

Optional tools:

    Tool          Purpose                            Install
    ----          -------                            -------
    XcodeGen      Regenerate Xcode project           brew install xcodegen
    Homebrew      Install CMake and XcodeGen          /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

## Quick Start

Clone the repository:

    git clone https://github.com/avwohl/phograph.git
    cd phograph

### Option A: Run the macOS App (Xcode)

    open Phograph.xcodeproj

Select the **Phograph** scheme and press **Cmd+R** to build and run. The app targets macOS 13+.

Or build from the command line:

    xcodebuild -scheme Phograph -destination 'platform=macOS' build

Once running:

1. **File > New Project** (Cmd+N) creates a starter project that computes `(3 + 4) * 2 = 14`
2. Click **Run** in the toolbar to execute
3. **Cmd+K** opens the fuzzy finder to add new nodes

### Option B: Build and Test the C++ Engine Only

    cd phograph_core
    cmake -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

This builds the portable C++ engine as a static library and runs the full test suite (13 test executables, ~200 assertions). No Xcode app or GUI required.

Expected output:

    100% tests passed, 0 tests failed out of 13

## Architecture

The project follows a portable C++ core with thin platform bridge pattern:

    phograph/                    macOS IDE + app
      Phograph/
        App/                     SwiftUI entry point
        Bridge/                  ObjC++ bridge to C++ engine
        Views/                   SwiftUI IDE views (canvas, browser, inspector, front panel)
          IDE/FrontPanelView.swift  Runtime front panel UI
        ViewModels/              IDE state management
        Model/                   Swift ObservableObject models
        Metal/                   MetalRenderer + shaders
        Runtime/                 Swift runtime for compiled programs

    phograph_core/               Portable C++ engine (no platform #includes)
      src/
        pho_value.{h,cc}         Tagged union for 13 types
        pho_graph.{h,cc}         Graph model: Node, Wire, Method, Case, Class
        pho_eval.{h,cc}          Dataflow evaluator/scheduler
        pho_serial.{h,cc}        JSON serialization/deserialization
        pho_bridge.{h,cc}        C API for Swift/ObjC interop
        pho_prim*.cc             Primitive implementations (~25 files)
        pho_prim_date.cc         Date/time primitives
        pho_prim_methodref.cc    Method reference primitives
        pho_scene.{h,cc}         Scene graph
        pho_draw.{h,cc}          CPU rasterizer
        pho_codegen.{h,cc}       Graph-to-Swift compiler
        pho_debug.{h,cc}         Debugger/trace
        pho_thread.{h,cc}        Run loop, timers, event queue
        pho_platform.h           Platform abstraction (no implementation)
        plugins/                 Native audio/MIDI plugin implementations
      tests/                     C++ test suite (13 executables)

    libraries/                   Plugin libraries (10 libraries, 135 primitives)
      math/                      factorial, fibonacci, gcd, lcm, is-prime
      crypto/                    SHA-256, HMAC, AES, random bytes, etc.
      image/                     Load/save PNG/JPEG, resize, pixel access
      sound/                     Audio playback, sample generation
      midi/                      MIDI I/O, note on/off, control change
      fileio/                    Read/write/list files and directories
      socket/                    TCP/UDP client/server
      net/                       HTTP get/post
      bitmap/                    Pixel buffer manipulation
      locale/                    Date/time formatting, locale info

    docs/                        Language spec, design docs
    site/                        GitHub Pages tutorial site

The C++ core has zero platform `#include`s. All I/O goes through `pho_platform.h`, which has per-platform implementations (Apple: ObjC++ in `Bridge/pho_platform_apple.mm`).

## Testing

### C++ Engine Tests

    cd phograph_core
    cmake -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

The test suite contains 13 executables:

    Test                What it covers
    ----                --------------
    test_eval           Core evaluator, JSON parser, basic graph execution
    test_phase2         String, list, dict, type, data, error primitives; multi-case dispatch; recursion
    test_phase3         OOP: classes, instance generators, get/set, inheritance
    test_bridge         C bridge API: load JSON, call methods, pixel buffer, error handling
    test_scene          Scene graph: shapes, transforms, hit testing, canvas, drawing
    test_phase6         Run loop: events, timers, input handling, easing, animation
    test_phase7         IDE rendering: fuzzy search, node layout, wire/grid rendering
    test_phase8         Debugger: breakpoints, traces, snapshots, step control
    test_phase9         Async: futures, channels, effects
    test_phase10        Compiler: Swift code generation, name mangling, topo sort
    test_e2e_compile    End-to-end: compile graph to Swift, run swiftc, verify output
    test_phase11_28     Phases 11-28: execution wires, loops, listMap, try/error, pattern matching, observables
    test_comprehensive  Full-language bridge test: all primitives, types, control flow, OOP, canvas

### Library Validation

    bash tests/test_libraries.sh

Checks that all 10 library manifests are valid JSON, primitives are unique, C++ registration functions exist, and primitive counts match expectations.

### macOS App

    xcodebuild -scheme Phograph -destination 'platform=macOS' build

## Regenerating the Xcode Project

The checked-in `Phograph.xcodeproj` is generated from `project.yml`. If you add or remove source files:

    brew install xcodegen   # one-time
    xcodegen generate

## Documentation

- [Learn Phograph](https://avwohl.github.io/phograph/) -- step-by-step tutorial covering dataflow basics through OOP and advanced patterns
- [IDE Guide](https://avwohl.github.io/phograph/guide.html) -- canvas navigation, keyboard shortcuts, debugger, and export
- [Language Reference](https://avwohl.github.io/phograph/reference.html) -- complete reference for all data types, primitives, and evaluation rules

The full language specification is also available at `docs/prograph_language.md` (~2650 lines).

## Library System

Phograph supports plugin libraries that add new primitives to the environment.

- **Install:** Place a library folder (containing `library.json` and implementation files) in `~/Library/Application Support/Phograph/Libraries/`
- **Manage:** Use **Libraries > Manage Libraries...** to view installed libraries, check versions, and toggle availability
- **Use:** Library primitives appear in the right-click context menu and fuzzy finder (Cmd+K) alongside built-in nodes
- **Create:** A library needs a `library.json` declaring its name, version, and primitives (with input/output counts). See `docs/prograph_language.md` for the library specification.

## Examples

Open the Example Browser with **Cmd+Shift+E** to explore built-in examples:

- **Basics** -- arithmetic, string ops, control flow
- **Lists** -- map, filter, sort, list comprehensions
- **Classes** -- OOP with inheritance, instance generators, get/set
- **Graphics** -- scene graph shapes, canvas rendering
- **Patterns** -- loops, spreads, broadcasts, error handling

## Contributing

Contributions are welcome.

1. Fork the repository
2. Create a feature branch (`git checkout -b my-feature`)
3. Make your changes -- follow existing code style (SwiftUI views, C++ core)
4. Run the C++ engine tests:

       cd phograph_core && cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure

5. Build the app (if you changed Swift/UI code):

       xcodebuild -scheme Phograph -destination 'platform=macOS' build

6. Open a pull request against `main`

## License

MIT License - see [LICENSE](LICENSE)

## Privacy

See [PRIVACY.md](PRIVACY.md)

## Author

Aaron Wohl

https://github.com/avwohl/phograph
