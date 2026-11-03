#!/bin/sh
# Build vtstub.dylib (VideoToolbox decoder slot probe, vtstub.c) into phone/video/out.
# Linux: tools/linux-ios-cc + ldid (like phone/audio/build.sh). Install: tools/sunfish-vtstub.sh install.
set -e
cd "$(dirname "$0")"
CC=${CC:-../../tools/linux-ios-cc}
mkdir -p out
$CC -O2 -Wall -dynamiclib -install_name /System/Library/VideoDecoders/ExampleIPB.videodecoder \
	-o out/vtstub.dylib vtstub.c
../../tools/linux-ios-sign out/vtstub.dylib
llvm-nm -gU out/vtstub.dylib | grep -q '_ExampleIPBDecoderRegister' || { echo "no ExampleIPBDecoderRegister export"; exit 1; }
ls -la out/vtstub.dylib
