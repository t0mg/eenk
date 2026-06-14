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
#   -static must come first to force static resolution of all subsequent libs
#   mingw32 → SDL2main → SDL2 → Windows system libs
# Static link: force all libs to be linked statically
env.Append(LINKFLAGS=["-static"])

# SDL2 and Windows system libraries needed for SDL2 static link
env.Append(LIBPATH=["C:/msys64/mingw64/lib"])
env.Append(LIBS=[
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



