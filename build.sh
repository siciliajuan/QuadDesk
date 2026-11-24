#!/bin/sh
# Build QuadDesk with Open Watcom C 2.0 (Linux or macOS).
# Requires the WATCOM environment variable pointing to the Open Watcom
# installation, e.g.:  export WATCOM=/opt/watcom
set -e

: "${WATCOM:?Set WATCOM to your Open Watcom installation directory}"
case "$(uname -s)" in
  Darwin) BIN="$WATCOM/bino64" ;;
  *)      BIN="$WATCOM/binl64" ;;
esac
[ -d "$BIN" ] || BIN="$WATCOM/binl"
export PATH="$BIN:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"

OUT=build
mkdir -p "$OUT"
cd "$OUT"

CFLAGS="-q -bt=windows -ml -zw -ox -w4 -i=../src"

echo "Building QDHOOK.DLL"
wcc $CFLAGS -bd -zu -fo=qdhook.obj ../src/qdhook.c
wlink op q system windows_dll file qdhook.obj name qdhook.dll
wlib -q -n qdhook.lib +qdhook.dll

echo "Building QUADDESK.EXE"
wcc $CFLAGS -fo=quaddesk.obj ../src/quaddesk.c
wlink op q system windows file quaddesk.obj library qdhook.lib \
      name quaddesk.exe option stack=8k option heapsize=4k

echo "Done: $OUT/quaddesk.exe and $OUT/qdhook.dll"
