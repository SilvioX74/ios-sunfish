# Building xnu on Linux (no macOS, no Xcode)

**Why:** this machine has no macOS VM, and the upstream CI stopped publishing kernel artifacts on 2026-10-28
(builds went local that day). The newest kernel we can run is therefore that morning's artifact, which predates
the amplifier bring-up (audio, same day 18:25), the camera stack (10-30) and the B29 fix (10-31). Building xnu
here is what unlocks all three. Started 2026-11-19 at the user's choice ("provo a compilare su Linux").

**Setup:** `tools/xnu-linux-setup.sh` (and `--source` to prepare the tree), then `source work/xnu-linux/env.sh`.
System packages first: `autoconf automake libtool bison flex m4 pkg-config ninja-build libssl-dev uuid-dev
zlib1g-dev`, plus git, cmake, make, clang, python3.

## What the toolchain is made of (all built from source into work/xnu-linux/tc)

| piece | from | what it is for |
|---|---|---|
| mig, migcom | markmentovai/bootstrap_cmds, branch `cross_platform` | Mach interface generator |
| ld, ar, as, strip, nm, otool, lipo, codesign_allocate, ctf_insert | tpoechtrager/cctools-port | Apple's binary tools; `ld64.lld` is not enough (no `-image_base`, `-segaddr`, `-segment_order`, `-alias_list`) |
| libdispatch, BlocksRuntime | tpoechtrager/apple-libdispatch | cctools' ld64 needs them; Ubuntu has no package |
| lzfse | lzfse/lzfse | kernelcache compression |
| unifdef | fanf2/unifdef | build with `CFLAGS=-std=gnu17`: its own source uses `constexpr` as a variable name, a keyword since C23 |
| tcsh | tcsh-org/tcsh | `SETUP/config/doconf` is a csh script |
| shims (xcrun, plutil, codesign, iig, sw_vers, sysctl, ditto, install...) | Allenwdk/xnu-linux-toolchain | stand in for Xcode; `iig` there is a Python reimplementation |

## The source tree (work/src/xnu)

`xnu-8796.141.3` from apple-oss-distributions, then **in this order**:

1. The three staging scripts beside our patches, which vendor Apple's own sources at pinned commits:
   `0019-stage-libpthread.sh`, `0020-stage-bearssl.sh`, `0026-stage-hfs.sh`. They must run before the patches
   and they call `xcrun`, so the shims and the fake SDK have to be in PATH already. (`git clean -fdx` does **not**
   remove what they stage: those trees carry their own `.git`, so use `-ffdx`.)
2. Our patch series: **280 of 281 apply cleanly**. The one left out is `0160-sunfish-iomfb-log-async`, which
   vendors a whole newer `iokit/` tree (286 files) over files that already exist; `docs/plan.md` already calls for
   "0125-0285 without 0160's vendored tree", so this matches upstream's own intent.
3. Four portability patches from Allenwdk/xnu-linux-toolchain, **all four apply cleanly** on top:
   `portable-kalloc-type-builtins` (the important one: xnu-8796 uses `__builtin_xnu_type_*`, which only Apple's
   clang has, in exactly one file, `osfmk/kern/kalloc.h`, so a fallback there avoids rebuilding clang),
   `linux-host-tools`, `iig-method-forwarders`, `linux-link-shims`.

## The fake SDK (work/xnu-linux/fakeroot)

A symlink farm over `research/sdk/theos-sdks/iPhoneOS16.5.sdk`, plus the Darwin headers that SDK lacks:
`compat/hostcompat.h` (xnu force-includes it by absolute path into every host tool), and, on `CPATH` rather than
in the SDK, `libc.h` and `copyfile.h` (the host tools include those as `<...>`, so they resolve against the host
include path, not the SDK).

> **Trap, learned the hard way on 2026-11-19:** an override must be a regular file. `cat > fakeroot/.../copyfile.h`
> followed the symlink and overwrote **Apple's header inside research/sdk**. Restored from
> `https://raw.githubusercontent.com/theos/sdks/master/iPhoneOS16.5.sdk/usr/include/copyfile.h` (4,546 bytes);
> it was the only file damaged. The setup script now deletes the symlink before writing.

## Status

**2026-11-19, end of the first session: the kernel compiles. Nine object files out of the whole tree do not.**

The build runs `DEVELOPMENT_ARM64_SWEET` to completion except for those nine, through roughly two million lines
of output: 3,237 headers installed, 87 MIG interfaces, 34 IIG (DriverKit) interfaces generated, every SETUP host
tool built, and the great majority of the kernel's C and assembly compiled. Walls passed, in order, each needing
its own fix: `compat/hostcompat.h`; `libc.h` and `copyfile.h` (host tools resolve those against the host include
path, so they go on CPATH, not in the SDK); the Mach-O headers (cctools' own `include/` and the pruned
`include/foreign`, which must NOT be added wholesale -- its `strings.h` and friends clash with glibc); the
Mach-O byte-swap functions and `NXGetLocalArchInfo` (libstuff, plus our own `archcompat.o`, because libstuff's
version asks the Mach kernel); `availability.pl`, which the iPhoneOS SDK has no copy of (replacement in the fake
SDK); `Kernel.framework/.../PrivateHeaders/platform`; and `-Werror`, which clang 21 trips over 17,362 times on
Apple's own exported typedefs (`BUILD_WERROR=0`, exported so the recursive makes inherit it).

What is left, by object file:

| object | problem |
|---|---|
| `sha256_compress_arm64.o` | 32 "invalid operand for instruction" from clang 21's integrated assembler on `osfmk/arm64/corecrypto/sha256_compress_arm64.s`. Allenwdk's answer is a **clang** patch (`llvm-clang-arm64-crypto-asm.patch`), so this one may force a patched compiler after all -- unless the macros can be rewritten or a C implementation substituted. |
| `hfs_cprotect.o`, `hfs_readwrite.o`, `hfs_vfsops.o`, `hfs_vfsutils.o` | `struct aks_cred_s` is dereferenced somewhere, so the opaque typedefs in our `EXTERNAL_HEADERS/AppleKeyStore/AppleKeyStoreFSServices.h` are not enough; `io_params` undeclared in two places. |
| `hfs_encodings.o` | `unicode_to_hfs_func_t` / `unicode_to_mac_roman` missing: the staged HFS expects an encoding piece that is not in the tree. |
| `log.o`, `subr_log.o` | not yet read. |
| `vm_compressor.o` | one error fixed (the CONFIG_JETSAM guard below); whether another remains is not yet known. |

Our own source changes live in `xnu/patches-linux/`, applied by the setup script after the portability patches:
the host-tool gaps Allenwdk's patch does not cover on xnu-8796 (`getprogname`, `setmode`/`getmode` in
installfile and replacecontents), the AppleKeyStore header Apple does not publish, and a guard in
`osfmk/vm/vm_compressor.c`, where the non-OSX branch uses `memorystatus_swap_all_apps` while its declaration
sits under `CONFIG_JETSAM`, which the SWEET config turns off -- a combination Apple never shipped.
