# What I found poking at Nickel

This is the write-up of the reverse-engineering that the rest of the project is
built on. Everything here was done *statically* — reading the binary, never
running anything on the device — so there was no risk to the reader at any
point. The star of the show is `libnickel.so.1.0.0`, the 46 MB library that is
basically the whole reader app, pulled out of the rootfs with `debugfs`.

One practical note up front: rea's disassembler and decompiler are switched off
on Windows, so I couldn't get pretty pseudo-code. That turned out not to matter
much — the questions that actually decide this project are answered by symbols
and linker metadata, which plain binutils (`nm -D`, `readelf -d`, `c++filt`)
read just fine.

## The device

From `/.kobo/version`: `...,4.9.77,5.18.279364,4.9.77,4.9.77,...0690`. So it's a
kernel 4.9.77 system, firmware `5.18.279364`, device-id `0x690`, on MediaTek
`mt8113t-ntx` hardware. In plain terms: a Tolino Vision Color, which is the same
hardware as a Kobo Libra Colour.

## The thing that changes everything: it's Qt6

The first real surprise. Looking at what `libnickel` is linked against:

```
libQt6Widgets.so.6   libQt6Gui.so.6   libQt6Svg.so.6
libQt6Multimedia.so.6   libQt6WebEngineCore.so.6   libQt6WebEngineWidgets.so.6
libQt6StateMachine.so.6   libQt6TextToSpeech.so.6
```

Qt**6**. Every classic Kobo mod I know of, NickelMenu included, is built for
Qt5. NickelMenu's trick is to disguise itself as a Qt5 image-format plugin
(`libnm.so`, dropped into `/usr/local/Kobo/imageformats/`) and let the Qt plugin
loader pick it up at startup. Qt6 won't touch a Qt5 plugin — the plugin ABI and
metadata changed between the two — so that library simply never loads, and the
mod never runs.

That single fact is, I'm fairly sure, the whole story behind the community's
vague "firmware 5.x isn't supported" line. It's not a mystery; it's the Qt5→Qt6
jump that this generation of readers made.

## The menu functions are still there (and it's a trap)

Here's the part that could fool you. The old menu-building functions are *still
exported* by `libnickel`, with the same signatures as always:

```
AbstractNickelMenuController::createMenuTextItem(QMenu*, QString const&, bool, bool, QString const&)   @ 0x01614d99
AbstractMenuController::createAction(QMenu*, QWidget*, bool, bool, bool)
MainWindowController::sharedInstance()
```

It's tempting to see those and think "great, NickelMenu will work." It won't —
because the problem was never the functions, it was getting *in the door* to
call them. The Qt6 plugin loader is the locked door. These symbols only start to
matter *after* something is already running inside the process and wants to build
a menu, which is the far-off Stage C.

## It's 32-bit ARM

`rea`'s `open_binary` reports the library as ELF, `arm`, 32-bit. So anything I
inject has to match: 32-bit ARM, hard-float (armv7-a). Worth knowing before
compiling anything.

## The way in: LD_PRELOAD

Nickel is started from `/etc/rc.local`:

```
33: source /usr/libexec/platform/nickel-env.sh
56: /usr/local/Kobo/nickel -platform kobo &
```

And `restart_nickel.sh` (the in-session restart path) sources the *same*
`nickel-env.sh`. That env file sets up `LD_LIBRARY_PATH`, picks `linuxfb` as the
Qt platform (nice side note: Nickel draws straight to the framebuffer, which is
exactly what `fbink` writes to), loads a pile of Qt6 WebEngine flags — but it
does **not** set `LD_PRELOAD`.

So the plan writes itself: export `LD_PRELOAD` in `rc.local`, after the env is
sourced and before Nickel launches, and the dynamic linker will pull our library
into the process. No Qt plugin system involved, so the whole Qt6 problem just
evaporates. Details of the wiring live in [`../scripts/wiring.md`](../scripts/wiring.md).

## Will our library even load? (yes)

A loaded library has to agree with the device's glibc. The cross-toolchain I used
is glibc 2.41, which sounds scary, but what matters is the *highest version our
tiny library actually references*: GLIBC_2.34. The device's own libc is 2.35.
2.35 ≥ 2.34, so it loads. (It helps that the Stage A code only calls ancient,
boring functions — `fopen`, `time`, `getpid` and friends.)

## The short version of the risk

- Getting code to simply *run* inside Nickel: doable, and low-risk — that's
  Stage A.
- A library that loads but then does something wrong can stop Nickel from
  starting. Recoverable (yank the opt-in flag over USB, or reflash stock), but
  annoying — so Stage A is deliberately inert.
- Calling Qt6 methods to add a genuine menu item (Stage C) means getting the Qt6
  ABI exactly right, with no decompiler to check my work on Windows. That's the
  real risk, and it waits until the earlier stages prove the ground is solid.
