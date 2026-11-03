#!/bin/sh
# Builds FH6ChaseCam.exe. Windows: run from MSYS2 MinGW64. Linux: needs mingw-w64.
#   sh editor/build.sh          -> editor/build/FH6ChaseCam.exe
#   sh editor/build.sh test     -> native core tests
#   sh editor/build.sh fovtest  -> editor/build/FH6FovTest.exe (FOV live-reload test)
set -e
cd "$(dirname "$0")"
mkdir -p build
if [ "$1" = "test" ]; then
  shift
  g++ -std=c++17 -O2 -Wall -Wextra -o build/core_test tests/core_test.cpp src/core.cpp src/camfiles.cpp src/schema.cpp src/layereval.cpp
  ./build/core_test "$@"
  exit 0
fi
CXX=${CXX:-x86_64-w64-mingw32-g++-posix}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
I=third_party/imgui
$WINDRES res/app.rc -O coff -o build/app.res
FLAGS="-std=c++17 -O2 -DNDEBUG -DUNICODE -D_UNICODE -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS -I$I -Isrc"
IMGUI="$I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp $I/imgui_impl_win32.cpp $I/imgui_impl_dx11.cpp"
LIBS="-municode -mwindows -static -s -ld3d11 -ld3dcompiler -ldxgi -ldwmapi -lws2_32 -lole32 -lshell32 -luuid -lgdi32 -limm32"
if [ "$1" = "fovtest" ]; then
  $CXX $FLAGS -DFOVTEST src/main.cpp src/fovtest.cpp src/theme.cpp src/core.cpp src/camfiles.cpp src/schema.cpp src/dataout.cpp \
    $IMGUI build/app.res $LIBS -o build/FH6FovTest.exe
  ls -la build/FH6FovTest.exe
  exit 0
fi
$CXX -std=c++17 -O2 -DNDEBUG -DUNICODE -D_UNICODE -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS -I$I -Isrc \
  src/main.cpp src/app.cpp src/pages.cpp src/theme.cpp src/preview.cpp src/carmesh.cpp src/sim.cpp src/core.cpp src/camfiles.cpp src/schema.cpp src/layereval.cpp src/dataout.cpp \
  $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp \
  $I/imgui_impl_win32.cpp $I/imgui_impl_dx11.cpp build/app.res \
  -municode -mwindows -static -s \
  -ld3d11 -ld3dcompiler -ldxgi -ldwmapi -lws2_32 -lole32 -lshell32 -luuid -lgdi32 -limm32 \
  -o build/FH6ChaseCam.exe
ls -la build/FH6ChaseCam.exe
