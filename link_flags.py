# Custom SCons script to append linker flags for static compilation in the native environment
import os
import sys
from typing import Any
from SCons.Script import Import

Import("env")

# Declare env for IDE static analyzers to prevent unresolved variable errors
env: Any

# Prepend MSYS2 MinGW64 to PATH in the build environment so compilation works out of the box on Windows
if os.name == 'nt':
    mingw_bin = "C:/msys64/mingw64/bin"
    msys_bin = "C:/msys64/usr/bin"
    if os.path.exists(mingw_bin):
        env["ENV"]["PATH"] = mingw_bin + os.pathsep + msys_bin + os.pathsep + env["ENV"].get("PATH", "")

# Static link order for MinGW + SDL2:
if os.name == 'nt':
    env["CC"] = "C:/msys64/mingw64/bin/gcc.exe"
    env["CXX"] = "C:/msys64/mingw64/bin/g++.exe"
    env.Append(LINKFLAGS=["-static"])
    env.Append(CPPPATH=["C:/msys64/mingw64/include/SDL2"])
    env.Append(LIBPATH=["C:/msys64/mingw64/lib"])
    env.Append(LIBS=[
        "mingw32",
        "SDL2main",
        "SDL2",
        "dinput8",
        "dxguid",
        "dxerr8",
        "user32",
        "gdi32",
        "winmm",
        "imm32",
        "ole32",
        "oleaut32",
        "shell32",
        "setupapi",
        "version",
        "uuid",
    ])
elif sys.platform == 'darwin':
    import subprocess
    prefix = None
    try:
        prefix = subprocess.check_output(["sdl2-config", "--prefix"]).decode().strip()
    except Exception:
        for p in ["/opt/homebrew", "/usr/local"]:
            if os.path.exists(p):
                prefix = p
                break

    try:
        cflags = subprocess.check_output(["sdl2-config", "--cflags"]).decode().strip().split()
        for flag in cflags:
            if flag.startswith("-I"):
                env.Append(CPPPATH=[flag[2:]])
            elif flag.startswith("-D"):
                env.Append(CPPDEFINES=[flag[2:]])
    except Exception:
        if prefix:
            env.Append(CPPPATH=[os.path.join(prefix, "include", "SDL2")])

    sdl2_a = None
    if prefix:
        candidate = os.path.join(prefix, "lib", "libSDL2.a")
        if os.path.exists(candidate):
            sdl2_a = candidate

    if not sdl2_a:
        for p in ["/opt/homebrew/lib/libSDL2.a", "/usr/local/lib/libSDL2.a"]:
            if os.path.exists(p):
                sdl2_a = p
                break

    if sdl2_a:
        static_flags = []
        try:
            static_libs = subprocess.check_output(["sdl2-config", "--static-libs"]).decode().strip().split()
            for token in static_libs:
                if token.startswith("-L") or token == "-lSDL2":
                    continue
                static_flags.append(token)
        except Exception:
            static_flags = [
                "-Wl,-framework,CoreVideo", "-Wl,-framework,Cocoa", "-Wl,-framework,IOKit",
                "-Wl,-framework,ForceFeedback", "-Wl,-framework,Carbon", "-Wl,-framework,CoreAudio",
                "-Wl,-framework,AudioToolbox", "-Wl,-framework,AVFoundation", "-Wl,-framework,Foundation",
                "-Wl,-framework,AppKit", "-Wl,-framework,GameController", "-Wl,-framework,Metal", "-liconv"
            ]

        sdl2main_a = os.path.join(os.path.dirname(sdl2_a), "libSDL2main.a")
        if os.path.exists(sdl2main_a):
            env.Append(LINKFLAGS=[sdl2main_a])

        env.Append(LINKFLAGS=[sdl2_a] + static_flags)
    else:
        try:
            env.ParseConfig("sdl2-config --cflags --libs")
        except Exception:
            if prefix:
                env.Append(CPPPATH=[os.path.join(prefix, "include", "SDL2")])
                env.Append(LIBPATH=[os.path.join(prefix, "lib")])
            env.Append(LIBS=["SDL2"])
else:
    # Linux
    import subprocess
    try:
        env.ParseConfig("sdl2-config --cflags --libs")
    except Exception as e:
        print("Fallback sdl2 config...")
        env.Append(CPPPATH=["/usr/include/SDL2"])
        env.Append(LIBPATH=["/usr/lib"])
        env.Append(LIBS=["SDL2"])
