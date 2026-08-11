#!/usr/bin/env python3
"""
scripts/build_version.py — PlatformIO pre-build version injection
=================================================================
Invoked automatically by PlatformIO before every compilation via:
    extra_scripts = pre:scripts/build_version.py   (in [common], platformio.ini)

Queries the local Git repository state and injects the result as a C preprocessor
macro so every compiled binary carries an immutable version string in flash (DROM).

Macro injected:
    EENK_VERSION_STR   — null-terminated string literal, e.g. "v0.1.0" or
                         "v0.1.0-3-ga1b2c3" (3 commits past tag) or
                         "v0.1.0-dirty" (uncommitted changes in working tree)

During unit testing (pio test), a static version string ("v0.0.0-test") is used
so golden screenshot tests (e.g. SettingsView) remain deterministic across commits.
"""

import os
import subprocess
import sys

Import("env")  # noqa: F821  (PlatformIO injects this into the SCons context)


def _get_git_version() -> str:
    """Return a version string derived from the nearest Git tag."""
    # Allow explicit environment variable override
    if os.environ.get("EENK_BUILD_VERSION"):
        return os.environ.get("EENK_BUILD_VERSION")

    # Detect unit testing builds (pio test) to ensure static version in golden screenshots
    is_test_build = (
        "test" in sys.argv
        or any("test" in arg for arg in sys.argv)
        or bool(os.environ.get("PIO_UNIT_TESTING"))
        or bool(env.get("PIO_UNIT_TESTING"))
        or "test" in [str(t) for t in env.get("BUILD_TARGETS", [])]
        or "test" in [str(t) for t in env.get("COMMAND_LINE_TARGETS", [])]
    )
    if is_test_build:
        return "v0.0.0-test"

    try:
        # --tags       : use lightweight tags too, not just annotated ones
        # --always     : fall back to short SHA if no tag exists at all
        # --dirty      : append "-dirty" if working tree has uncommitted changes
        raw = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL,
        )
        return raw.decode("utf-8").strip()
    except Exception:
        # Git not available or repo has no commits — safe fallback.
        return "v0.0.0-unknown"


version_str = _get_git_version()
print(f"*** eenk firmware version: {version_str} ***")

# Inject as a C string literal macro.
# The double-escaping is required so the macro expands to a quoted string in C:
#   -DEENK_VERSION_STR=\"v0.1.0\"   →   const char* v = EENK_VERSION_STR;
env.Append(  # noqa: F821
    CPPDEFINES=[
        ("EENK_VERSION_STR", f'\\"{ version_str }\\"'),
    ]
)

try:
    Import("projenv")
    projenv.Append(
        CPPDEFINES=[
            ("EENK_VERSION_STR", f'\\"{ version_str }\\"'),
        ]
    )
except Exception:
    pass

global_env = DefaultEnvironment()
global_env.Append(
    CPPDEFINES=[
        ("EENK_VERSION_STR", f'\\"{ version_str }\\"'),
    ]
)
