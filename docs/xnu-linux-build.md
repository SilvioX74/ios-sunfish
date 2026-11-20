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

## The one thing that bites over and over: our SDK is iPhoneOS, upstream's is macOS

Upstream builds with `MACOS_VERSION=13.5` and the macOS SDK, so `TARGET_OS_IPHONE` is 0 and `PLATFORM` is
MacOSX. The only Apple SDK on this machine is iPhoneOS 16.5, so both flip, and three separate failures came
from that single difference:

- `hfs_encodings.h` hides its whole body behind `#if !TARGET_OS_IPHONE`, so the HFS converter typedefs vanished.
- `kext-config.h` turns `CONFIG_PROTECT` on under `TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR`, which drags in
  AppleKeyStore -- a keystore this kernel does not have. The project's own `sweet_hfs_compat.h` says so in
  plain words ("SWEET has no AppleKeyStore, so content protection stays off"); it just never had to force it.
- `MakeInc.def` enables the minimal UBSan runtime for DEVELOPMENT only `ifeq ($(PLATFORM),iPhoneOS)`, while the
  SWEET config deliberately leaves `ubsan_minimal` out, so every `__ubsan_handle_*_minimal` was undefined.

Setting `PLATFORM=MacOSX` is not the answer: the toolchain's `target-clang` shim treats
`-mmacosx-version-min` as the signal for a host-tool build and hands the compile to the host compiler, so the
kernel ended up being compiled for x86_64 Linux. Each site is forced individually instead.

## Status

**2026-11-20: the kernel builds. `mach.development.sweet`, 16,176,472 bytes, Mach-O 64-bit arm64, Darwin
Kernel Version 22.6.0, built on Linux with no macOS and no Xcode.** Installed as `work/kernel285/` with its
provenance; patch level 0285, which is 89 patches ahead of the 0196 CI artifact the phone runs today.

Verified in the binary: `sweetsurfalign`, `sweetgpu`, `sweetmtl`, `sweetwifiev`, `sweetrtcoff`, `flame_wlan`,
`SweetKeyStore`, and the compiled-in default boot-arg string from xnu 0244 (which carries `sweetgpufreq=6
sweethwcg=1 sweetsmmutcr...`, newer than 0196's).

It has **not been booted**. The first boot of a never-tried kernel can hang the phone into a manual power-cycle,
so it waits for the user to be at the phone.

What the build still does not do: `make install`'s last step, `check_all_exports`, fails with "exported name
not in import list" for IOKit C++ symbols (`IOService::Stop_async`, `IOFilterInterruptEventSource::...`). That
is the KPI symbol-set validation for loadable kexts, run after the kernel is linked, stripped and dSYM'd --
the kernel binary is already complete at that point. The likely cause is the Python `iig` reimplementation
emitting a slightly different set of DriverKit stubs than Apple's. Nothing we boot depends on it.

### The order of the fixes, for whoever repeats this

| wall | fix |
|---|---|
| `compat/hostcompat.h` | written into the fake SDK; `hostcompat_init` is a no-op there, so `-lhostcompat` can go |
| `libc.h`, `copyfile.h` | host tools resolve `<...>` against the host path: ours go on `CPATH`, never into the SDK |
| Mach-O headers | cctools' `include/`, plus a **pruned** `include/foreign` (whole, its `strings.h` clashes with glibc) |
| byte swaps, `NXGetLocalArchInfo` | `libstuff.a` plus our `hostlib/archcompat.o` (libstuff's own asks the Mach kernel) |
| `availability.pl` | replacement in the fake SDK; it only has to print OS version lists |
| `Kernel.framework/.../PrivateHeaders/platform` | created empty |
| `-Werror` | `BUILD_WERROR=0`, exported: clang 21 raises 17,362 `-Wreserved-identifier` on Apple's own typedefs |
| SETUP host tools | `getprogname`, `setmode`/`getmode`, `fcopyfile`, `mach_vm_round_page` (xnu/patches-linux) |
| corecrypto SHA-256 assembly | `arm64_isa_compatibility.h` asked for Apple's clang before using the canonical `v<N>.4s` crypto syntax; any non-Apple clang now takes that branch |
| `os/firehose_buffer_private.h` | from libdispatch **1415.140.2** (the macOS 13.5 pairing; `main` wants a `firehose_stream_metric` this xnu lacks), with its guard widened to accept `KERNEL` like xnu's own firehose headers |
| `-lfirehose_kernel` | `work/xnu-linux/build-firehose.sh`: two MIG headers plus one source file, compiled against xnu's *installed* headers, never the SDK's user-space Mach ones |
| `dsymutil` | `/usr/bin/dsymutil` (LLVM's); the shim pointed at a path that does not exist |
