#!/bin/sh
# Draw the reading stats on the e-ink display with fbink.
#
# Called by the in-Nickel hook (hook.c). Touches no Qt. Reads the library with
# the sqlite3 CLI (the device Python has no sqlite3 module) and draws with the
# bundled static fbink. Keeps no file open on /mnt/onboard (short writes only),
# so it never blocks the USB "Connect to PC" unmount. Leaves diagnostics in the
# log and a drive-readable copy of the text.

LOG=/tmp/hook-alive.log
OUT=/mnt/onboard/.custom/last-stats.txt
DB=/mnt/onboard/.kobo/KoboReader.sqlite
FBINK=/opt/custom/fbink
SQLITE=/usr/bin/sqlite3

echo "render: start" >> "$LOG"

summary=$("$SQLITE" -readonly "$DB" "
  SELECT 'Reading stats' || char(10) ||
         count(*) || ' books - ' ||
         sum(ReadStatus=1) || ' reading - ' ||
         sum(ReadStatus=2) || ' done'
  FROM content WHERE ContentType=6;" 2>>"$LOG")

books=$("$SQLITE" -readonly "$DB" "
  SELECT printf('%3d%%  %-7s  %s',
    COALESCE(___PercentRead,0),
    CASE ReadStatus WHEN 2 THEN 'done' WHEN 1 THEN 'reading' ELSE 'new' END,
    substr(COALESCE(Title,'(untitled)'),1,32))
  FROM content WHERE ContentType=6
  ORDER BY (DateLastRead IS NULL), DateLastRead DESC LIMIT 8;" 2>>"$LOG")

text="$summary

$books"
[ -n "$summary" ] || text="reading stats unavailable"
echo "render: built $(printf '%s\n' "$text" | wc -l) lines" >> "$LOG"

if [ ! -x "$FBINK" ]; then
    echo "render: fbink missing at $FBINK" >> "$LOG"
else
    "$FBINK" -q -c
    echo "render: fbink clear exit=$?" >> "$LOG"
    row=3
    printf '%s\n' "$text" | while IFS= read -r line; do
        "$FBINK" -q -y "$row" "$line"
        row=$((row + 2))
    done
    echo "render: draw finished exit=$?" >> "$LOG"
fi

{ echo "=== last render ==="; printf '%s\n' "$text"; } > "$OUT" 2>/dev/null
echo "render: wrote $OUT" >> "$LOG"
