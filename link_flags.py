# Custom SCons script to append linker flags for static compilation in the native environment
import os
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
else:
    # Use sdl2-config for macOS/Linux
    import subprocess
    try:
        env.ParseConfig("sdl2-config --cflags --libs")
    except Exception as e:
        print("Fallback sdl2 config...")
        env.Append(CPPPATH=["/opt/homebrew/include/SDL2"])
        env.Append(LIBPATH=["/opt/homebrew/lib"])
        env.Append(LIBS=["SDL2"])




