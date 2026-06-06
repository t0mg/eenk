# Custom SCons script to append linker flags for static compilation in the native environment
from typing import Any
from SCons.Script import Import

Import("env")

# Declare env for IDE static analyzers to prevent unresolved variable errors
env: Any

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



