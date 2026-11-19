#!/bin/bash
# Build the toolchain that compiles xnu-8796.141.3 on Linux, with no macOS and no Xcode.
# Written 2026-11-19 after doing it by hand; see docs/xnu-linux-build.md for what each piece is for.
#
#   tools/xnu-linux-setup.sh            build everything into work/xnu-linux/tc
#   tools/xnu-linux-setup.sh --source   only fetch and prepare work/src/xnu
#
# System packages needed first (the user installed these on 2026-11-19):
#   autoconf automake libtool bison flex m4 pkg-config ninja-build libssl-dev uuid-dev zlib1g-dev
# plus git, cmake, make, clang and python3.
set -euo pipefail
cd "$(dirname "$0")/.."
R=$PWD
W=$R/work/xnu-linux
TC=$W/tc
SRC=$R/work/src/xnu
J=${JOBS:-8}
clone() { [ -d "$2/.git" ] || git clone --depth 1 ${3:+--branch "$3"} -q "$1" "$2"; }

prepare_source() {
	clone https://github.com/apple-oss-distributions/xnu.git "$SRC" xnu-8796.141.3
	cd "$SRC"
	git checkout -q -B hakios
	git checkout -q -- . && git clean -ffdxq	# -ff: the staged trees carry their own .git
	# The three staging scripts pull Apple's libpthread, BearSSL and HFS into the tree. They run
	# BEFORE the patches and they call xcrun, so the shims and the fake SDK must already be in PATH.
	. "$W/env.sh"
	for s in 0019-stage-libpthread 0020-stage-bearssl 0026-stage-hfs; do
		MACHINE_CONFIG=SWEET MACOS_VERSION=13.5 bash "$R/xnu/patches/$s.sh"
	done
	local ok=0 skipped=""
	for f in "$R"/xnu/patches/*.patch; do
		if git apply "$f" 2>/dev/null; then ok=$((ok+1)); else skipped="$skipped $(basename "$f" .patch)"; fi
	done
	# 0160 vendors a whole newer iokit/ tree (286 files) over files that already exist; docs/plan.md
	# calls for "0125-0285 without 0160's vendored tree", so it is expected to be left out here.
	echo "patches applied: $ok, left out:$skipped"
	for p in portable-kalloc-type-builtins linux-host-tools iig-method-forwarders linux-link-shims; do
		git apply "$W/allenwdk/patches/xnu/$p.patch"
	done
	git add -A && git -c user.email=noreply@anthropic.com -c user.name=hakios commit -q \
		-m "xnu-8796.141.3 + staged libpthread/bearssl/hfs + project patches + Linux portability patches"
}

mkdir -p "$W/src" "$TC/bin" "$TC/cctools"
clone https://github.com/Allenwdk/xnu-linux-toolchain.git "$W/allenwdk"	# shims + the 4 portability patches

if [ "${1:-}" != "--source" ]; then
	# mig/migcom: Mach interface generator, Linux port of Apple's bootstrap_cmds
	clone https://github.com/markmentovai/bootstrap_cmds.git "$W/src/mig" cross_platform
	( cd "$W/src/mig" && autoreconf -fi >/dev/null && ./configure --prefix="$TC" >/dev/null && make -j"$J" >/dev/null
	  install -m755 migcom.tproj/migcom "$TC/bin/migcom"; install -m755 migcom.tproj/mig.sh "$TC/bin/mig" )

	# libdispatch + BlocksRuntime: cctools-port's ld64 needs them and Ubuntu has no package
	clone https://github.com/tpoechtrager/apple-libdispatch.git "$W/src/libdispatch"
	cmake -S "$W/src/libdispatch" -B "$W/src/libdispatch/build" -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$TC" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
	cmake --build "$W/src/libdispatch/build" -j"$J" >/dev/null && cmake --install "$W/src/libdispatch/build" >/dev/null

	clone https://github.com/lzfse/lzfse.git "$W/src/lzfse"
	cmake -S "$W/src/lzfse" -B "$W/src/lzfse/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
	cmake --build "$W/src/lzfse/build" -j"$J" >/dev/null && install -m755 "$W/src/lzfse/build/lzfse" "$TC/bin/lzfse"

	# unifdef: its own source uses `constexpr` as a variable name, a keyword since C23 (gcc 15 default)
	clone https://github.com/fanf2/unifdef.git "$W/src/unifdef"
	make -C "$W/src/unifdef" CFLAGS="-std=gnu17 -O2" -j"$J" >/dev/null
	install -m755 "$W/src/unifdef/unifdef" "$TC/bin/unifdef"

	# tcsh: xnu's SETUP/config/doconf is a csh script
	clone https://github.com/tcsh-org/tcsh.git "$W/src/tcsh"
	( cd "$W/src/tcsh" && ./configure --prefix="$TC" >/dev/null && make -j"$J" >/dev/null && install -m755 tcsh "$TC/bin/tcsh" )
	ln -sf tcsh "$TC/bin/csh"

	# cctools-port: Apple's ld64, ar, as, strip, nm, otool, lipo, codesign_allocate, ctf_insert
	clone https://github.com/tpoechtrager/cctools-port.git "$W/src/cctools-port"
	( cd "$W/src/cctools-port/cctools" && CC=clang CXX=clang++ CPPFLAGS="-I$TC/include" \
	  LDFLAGS="-L$TC/lib -Wl,-rpath,$TC/lib" ./configure --prefix="$TC/cctools" >/dev/null
	  make -j"$J" >/dev/null && make install >/dev/null )

	for f in "$W"/allenwdk/toolchain/*; do [ -f "$f" ] && cp -f "$f" "$TC/bin/"; done
	chmod +x "$TC"/bin/* 2>/dev/null || true
	ln -sf tcsh "$TC/bin/csh"
fi

# The fake SDK: a symlink farm over the iPhoneOS 16.5 SDK we already have, plus the one macOS header
# xnu force-includes into every host tool (compat/hostcompat.h), which the iOS SDK does not carry.
SDK=$R/research/sdk/theos-sdks/iPhoneOS16.5.sdk
FR=$W/fakeroot
rm -rf "$FR"; mkdir -p "$FR/usr/include/compat"
for e in "$SDK"/*; do b=$(basename "$e"); [ "$b" = usr ] || ln -sfn "$e" "$FR/$b"; done
for e in "$SDK"/usr/*; do b=$(basename "$e"); [ "$b" = include ] || ln -sfn "$e" "$FR/usr/$b"; done
for e in "$SDK"/usr/include/*; do b=$(basename "$e"); [ "$b" = compat ] || ln -sfn "$e" "$FR/usr/include/$b"; done
# Overrides must be REGULAR FILES. `cat >` through a symlink writes into the real SDK: that is how
# Apple's copyfile.h in research/sdk was overwritten on 2026-11-19 (restored from theos/sdks).
rm -f "$FR/usr/include/libc.h" "$FR/usr/include/copyfile.h"
cat > "$FR/usr/include/compat/hostcompat.h" <<'H'
/* hakios: the macOS SDK header xnu force-includes into its host tools; absent from the iPhoneOS SDK. */
#ifndef _COMPAT_HOSTCOMPAT_H_
#define _COMPAT_HOSTCOMPAT_H_
#ifdef __cplusplus
extern "C" {
#endif
extern void hostcompat_init(const char *progname);
#ifdef __cplusplus
}
#endif
#endif
H
cat > "$FR/usr/include/libc.h" <<'H'
/* hakios: Darwin's umbrella header, absent on Linux and in the iPhoneOS SDK. */
#ifndef _HAKIOS_LIBC_H_
#define _HAKIOS_LIBC_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__linux__) && !defined(getprogname)
extern char *program_invocation_short_name;
static inline const char *getprogname(void) { return program_invocation_short_name; }
#endif
#endif
H
# copyfile.h: the iPhoneOS SDK has the header but Linux has no implementation to link, so ours does the
# copy inline for the one call site in SETUP/installfile.
sed -n '/^\/\* hakios: Darwin.s copyfile/,$p' "$FR_COPYFILE_SRC" > "$FR/usr/include/copyfile.h" 2>/dev/null || true

cat > "$W/env.sh" <<ENV
R=$R
export TC=\$R/work/xnu-linux/tc
export UNIXNU_SDKROOT=\$R/work/xnu-linux/fakeroot
export PATH=\$TC/bin:\$TC/cctools/bin:\$PATH
export LD_LIBRARY_PATH=\$TC/lib:\${LD_LIBRARY_PATH:-}
ENV
[ "${1:-}" = "--source" ] && prepare_source
echo "toolchain in $TC; source work/xnu-linux/env.sh before building"
