// eenk_version.h — firmware version string, cross-platform
//
// build_version.py (PlatformIO pre:extra_scripts) injects EENK_VERSION_STR as
// a compiler flag from `git describe --tags --always --dirty` before every build.
//
// This header provides a safe fallback for environments that run without the
// pre-build script (e.g. test runners, IDEs opening files directly).
//
// Usage:
//   #include "eenk_version.h"
//   drawText(EENK_VERSION_STR);   // "v0.1.0"  on a clean tag build

#pragma once

#ifndef EENK_VERSION_STR
#define EENK_VERSION_STR "dev"
#endif
