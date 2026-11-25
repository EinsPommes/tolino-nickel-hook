# Hooking the preload into Nickel

Short version: we get our library loaded by asking the dynamic linker to preload
it, right before Nickel starts. Here's the where and the how — and why it's safe
to back out of.

## Where Nickel starts

It's launched from `/etc/rc.local` in the rootfs:

```
33: source /usr/libexec/platform/nickel-env.sh
...
56: /usr/local/Kobo/nickel -platform kobo &
```

The in-session restart path (`restart_nickel.sh`) sources the *same*
`nickel-env.sh`, which is handy: anything we export before the launch applies
whether the reader cold-boots or just restarts Nickel.

## The actual change (design — not applied anywhere yet)

Drop this block in *after* line 33 (the env is sourced) and *before* line 56
(Nickel launches):

```sh
# --- tolino-nickel-hook (opt-in, experimental) ---
if [ -f /mnt/onboard/.custom/ENABLE_HOOK ] && [ -f /opt/custom/libtnh.so ]; then
    export LD_PRELOAD="/opt/custom/libtnh.so${LD_PRELOAD:+:$LD_PRELOAD}"
fi
# --- end tolino-nickel-hook ---
```

Three small things make this hard to shoot yourself in the foot with:

- It only does anything if **you** put an `ENABLE_HOOK` file in `.custom/` on the
  USB drive. That's the on switch. Deleting the file over USB is the off switch —
  no reflash needed, which matters a lot while experimenting.
- It also checks the library is actually there, so a rootfs without it never ends
  up pointing `LD_PRELOAD` at a ghost.
- If you already had an `LD_PRELOAD` set, this keeps it and just adds ours in
  front.

## Please don't test this on the frozen package

`Desktop\update` (the v2.2.0 package) and `kobo-wifi-book-upload` are frozen —
they're going out as a release, untouched. So when Stage A is ready for a real
on-device try, the move is to spin up a **separate** rootfs working copy, bake
`libtnh.so` into `/opt/custom/` and add the block above *there*, and flash that
throwaway test package. The release stays clean.

One gotcha worth spelling out: the existing drive-override
(`/mnt/onboard/.custom/startup.sh`) runs *after* Nickel is already up, so it
can't set environment variables for Nickel's own launch. That's why the preload
can't be tested from the USB drive alone — it genuinely needs the `rc.local`
change baked into a rootfs.
