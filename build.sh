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
wcc $CFLAGS -fo=pager.obj ../src/pager.c
wcc $CFLAGS -fo=settings.obj ../src/settings.c
wcc $CFLAGS -fo=prefs.obj ../src/prefs.c
wcc $CFLAGS -fo=ctl3d.obj ../src/ctl3d.c
wrc -q -r -bt=windows -i=../src -fo=quaddesk.res ../src/quaddesk.rc
wlink op q system windows file quaddesk.obj file pager.obj file settings.obj \
      file prefs.obj file ctl3d.obj library qdhook.lib name quaddesk.exe \
      option stack=8k option heapsize=4k option resource=quaddesk.res

echo "Building QUADDESK.CPL"
DFLAGS="$CFLAGS -bd -zu"
wcc $DFLAGS -fo=cpl.obj ../src/cpl.c
wcc $DFLAGS -fo=settingsd.obj ../src/settings.c
wcc $DFLAGS -fo=prefsd.obj ../src/prefs.c
wcc $DFLAGS -fo=ctl3dd.obj ../src/ctl3d.c
wrc -q -r -bt=windows -i=../src -fo=cpl.res ../src/cpl.rc
wlink op q system windows_dll file cpl.obj file settingsd.obj file prefsd.obj \
      file ctl3dd.obj name quaddesk.cpl option resource=cpl.res \
      option description 'CPL:QuadDesk' option modname=QDESKCPL

echo "Building QDQA.EXE (QA tool, not part of QuadDesk)"
wcc $CFLAGS -fo=qdqa.obj ../tests/qdqa/qdqa.c
wlink op q system windows file qdqa.obj name qdqa.exe option stack=8k option heapsize=4k

echo "Building QDCPLT.EXE (CPL test tool, not part of QuadDesk)"
wcc $CFLAGS -fo=cpltest.obj ../tests/cpltest/cpltest.c
wlink op q system windows file cpltest.obj name qdcplt.exe option stack=8k option heapsize=4k

echo "Done: $OUT/quaddesk.exe, $OUT/qdhook.dll, $OUT/quaddesk.cpl and $OUT/qdqa.exe"
