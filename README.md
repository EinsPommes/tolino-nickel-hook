# tolino-nickel-hook

This is the experimental corner. It's where I poke at running my own code
*inside* the reader app (Nickel) on a Tolino Vision Color. If you came here
looking for the safe, boring, "just works" WiFi book uploader — that's a
different project (`kobo-wifi-book-upload`), and nothing here touches it.

**A fair warning before you read on:** this injects a native library into
Nickel. Get it wrong and Nickel might refuse to start — a black screen until you
either pull the opt-in flag off the USB drive or reflash the stock firmware.
Nothing here is dangerous *yet* (the current code does almost nothing on
purpose), but the whole point of the project is to walk toward something that
could be. So: eyes open.

## Why this exists

I wanted a reading-stats screen *on the device*, not just on my phone. The
classic way to add things to a Kobo's menus is NickelMenu — except it doesn't
load on this device at all. It turns out these newer Tolino/Kobo readers moved
to **Qt6**, and NickelMenu is a Qt5 plugin, so the app just never picks it up.
(The full detective story is in [`docs/findings.md`](docs/findings.md).)

The good news from that investigation: there's another door in. `LD_PRELOAD`
doesn't care about Qt versions at all — it's a plain dynamic-linker trick, and we
own the script that launches Nickel, so we can use it. That's the path this
project explores.

## Where things stand

The plan is to go one careful step at a time, proving each one before risking
the next:

- **Recon — done.** Figured out it's Qt6, found the symbols, found exactly where
  Nickel gets launched. All in `docs/findings.md`.
- **Stage A — proof of life. DONE (2026-11-25).** A tiny library that does one
  thing: write a line to a log file so I can confirm "yes, my code ran inside
  Nickel." No Qt, no menus, nothing that can crash the UI. Flashed and verified
  on-device — `hook-alive.log` showed the constructor running in the Nickel
  process (and, as expected, every child it spawns: ~23 PIDs). So LD_PRELOAD
  into a Qt6 Nickel genuinely works. Next hook must scope itself to the real
  Nickel process before acting.
- **Stage B — a cheap trigger.** Watch for some simple in-process event and
  launch a full-screen stats page drawn with `fbink`.
- **Stage C — a real menu entry.** The dream: an actual item in Nickel's menu.
  This needs Qt6 ABI work and is where the real risk lives, so it stays firmly
  on the "later, maybe" shelf until A and B earn it.

Only Stage A is on the table today.

## How I keep this from bricking the reader

- **It's opt-in.** The preload only kicks in if a file called `ENABLE_HOOK`
  exists in `.custom/` on the USB drive. Want it off? Delete that file over USB.
  No reflash, no drama.
- **A missing library is harmless.** If the `.so` isn't there, glibc just shrugs
  and starts Nickel normally. The thing that actually bites is a library that
  loads but then misbehaves — which is exactly why Stage A does nothing but
  append a log line.
- **There's always a way back.** A clean copy of the stock firmware is kept
  (frozen, outside this folder) for a full reflash if it ever comes to that.

## What's in here

```
src/hook.c          Stage A: the proof-of-life library (libc only, no Qt)
scripts/build.sh    cross-compiles it to 32-bit ARM
scripts/wiring.md   how the preload gets hooked into Nickel's launch (opt-in)
docs/findings.md    the reverse-engineering write-up behind all of this
```

## Building it

You'll need an ARM hard-float cross-compiler:

```
sudo apt-get install gcc-arm-linux-gnueabihf
```

Then run `scripts/build.sh`. It drops `build/libtnh.so` (not committed — it's a
build artifact). The library targets 32-bit ARM and leans only on the device's
own glibc, which was checked to be new enough.
