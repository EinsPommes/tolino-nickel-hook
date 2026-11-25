#!/usr/bin/env python3
"""Print a compact reading-stats summary for the e-ink renderer.

Called by show-stats.sh. Reads Nickel's library database read-only (no writes,
no Qt) and prints a few plain ASCII lines that fbink can draw. Python 3 and the
stdlib sqlite3 module are already on the device.
"""

from __future__ import annotations

import sqlite3
from pathlib import Path

DB_PATH = Path("/mnt/onboard/.kobo/KoboReader.sqlite")
BOOK_CONTENT_TYPE = 6
STATUS_LABELS = {0: "new", 1: "reading", 2: "done"}
MAX_TITLE = 32
MAX_ROWS = 8


def read_books() -> list[tuple[str, int, int]] | None:
    """Return (title, percent, status) rows, or None if the DB is unreadable."""
    if not DB_PATH.exists():
        return None
    try:
        with sqlite3.connect(f"file:{DB_PATH}?mode=ro", uri=True, timeout=5) as db:
            return db.execute(
                "SELECT Title, ___PercentRead, ReadStatus FROM content "
                "WHERE ContentType = ? "
                "ORDER BY (DateLastRead IS NULL), DateLastRead DESC",
                (BOOK_CONTENT_TYPE,),
            ).fetchall()
    except sqlite3.Error:
        return None


def clamp_percent(value) -> int:
    try:
        return max(0, min(100, int(value or 0)))
    except (TypeError, ValueError):
        return 0


def main() -> None:
    books = read_books()
    if books is None:
        print("reading stats: library database unavailable")
        return

    total = len(books)
    reading = sum(1 for _, _, status in books if status == 1)
    finished = sum(1 for _, _, status in books if status == 2)

    print("Reading stats")
    print(f"{total} books - {reading} reading - {finished} done")
    print("")
    for title, percent, status in books[:MAX_ROWS]:
        name = (title or "(untitled)")[:MAX_TITLE]
        label = STATUS_LABELS.get(status, "?")
        print(f"{clamp_percent(percent):3d}%  {label:7s}  {name}")


if __name__ == "__main__":
    main()
