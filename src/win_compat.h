// win_compat.h - POSIX compatibility shims for MSVC
// Force-included via CMake to fix core source files that use POSIX APIs.

#pragma once

#ifdef _MSC_VER

// pho_codegen.cc uses std::queue but doesn't include <queue>
#include <queue>

#endif // _MSC_VER
