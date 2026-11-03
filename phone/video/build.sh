#!/bin/sh
# Build the VideoToolbox decoder slot files into phone/video/out (Linux: tools/linux-ios-cc + ldid, like
# phone/audio/build.sh); both are /System/Library/VideoDecoders/ExampleIPB.videodecoder, one at a time:
#   vtstub.dylib   slot probe (vtstub.c), tools/sunfish-vtstub.sh install
#   vtvenus.dylib  video 4b: the H.264 decoder (4e: + HEVC, ../wifi/venus_hevc.c), a venusd client (vtvenus.c),
#                  tools/sunfish-install-vtvenus.sh install
set -e
cd "$(dirname "$0")"
CC=${CC:-../../tools/linux-ios-cc}
mkdir -p out
$CC -O2 -Wall -dynamiclib -install_name /System/Library/VideoDecoders/ExampleIPB.videodecoder \
	-o out/vtstub.dylib vtstub.c
$CC -O2 -Wall -Wextra -Wno-unused-parameter -dynamiclib -install_name /System/Library/VideoDecoders/ExampleIPB.videodecoder \
	-framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework VideoToolbox \
	-o out/vtvenus.dylib vtvenus.c ../wifi/venus_hevc.c
for f in vtstub vtvenus; do
	../../tools/linux-ios-sign out/$f.dylib
	llvm-nm -gU out/$f.dylib | grep -q '_ExampleIPBDecoderRegister' || { echo "$f: no ExampleIPBDecoderRegister export"; exit 1; }
done
ls -la out/vtstub.dylib out/vtvenus.dylib
