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

2026-11-19, end of the first session. The build reaches the SETUP phase -- the host tools xnu builds for itself
(config, decomment, installfile, kextsymboltool, replacecontents, setsegname, json_compilation_db) -- and stops
there. Three walls were passed in turn: `compat/hostcompat.h` (fake SDK), `libc.h` and `copyfile.h` (CPATH), and
the Mach-O headers (cctools' own include tree). Nothing of the kernel proper has been compiled yet, so nothing
can be claimed about that part.

What is left on the host tools, all small and all Darwin-specific:

| file | missing |
|---|---|
| setsegname.c | `mach-o/swap.h` |
| kextsymboltool.c | `mach/mach_init.h` |
| replacecontents.c | `getprogname` |
| installfile.c | `setmode`, `getmode` (BSD, in libbsd), `fcopyfile`, `getprogname` |

Allenwdk's `linux-host-tools.patch` is meant for exactly this but is written against their fork's versions of
these files, so it applies without fixing ours. The next step is a small host-tool patch of our own for
xnu-8796: point CPATH at cctools' include tree for the Mach headers, declare `getprogname`, and replace
`setmode`/`getmode`/`fcopyfile` in installfile.c. A few dozen lines across four files.
