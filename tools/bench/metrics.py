"""Download timeline metrics shared by bench.py and real-network runs.

A timeline is a list of (seconds since the task was added, completed bytes)
samples taken at a fixed short interval until the download finished.
"""

from __future__ import annotations

from statistics import median

WINDOW = 2.0  # seconds of the moving rate
DIP_RATIO = 0.5  # a dip is a rate below this share of the steady rate ...
DIP_SECONDS = 3.0  # ... lasting at least this long


def _rates(samples: list[tuple[float, int]]) -> list[tuple[float, int, float]]:
    """(time, completed, bytes/s over the preceding WINDOW) per sample."""
    out = []
    start = 0
    for t, done in samples:
        while samples[start][0] < t - WINDOW:
            start += 1
        t0, d0 = samples[start]
        out.append((t, done, (done - d0) / (t - t0) if t > t0 else 0.0))
    return out


def _first(samples, total, share):
    return next((t for t, done in samples if done >= total * share), None)


def summarize(samples: list[tuple[float, int]], total: int) -> dict:
    """Cold start, ramp, steadiness and tail of one download.

    ttfb      first payload byte
    ramp80    moving rate first reaches 80% of the steady rate
    steady    median moving rate while 10%..90% of the bytes arrive (the
              last third of a run stopped before 10%)
    peak      highest moving rate
    dips      spells of at least DIP_SECONDS below DIP_RATIO * steady
              between 5% and 95% of the bytes
    maxgap    longest time without progress after the first byte
    tail95/99 time from 95% / 99% of the bytes to the end
    """
    if not samples or total <= 0:
        return {}
    end = samples[-1][0]
    rates = _rates(samples)
    ttfb = next((t for t, done in samples if done > 0), None)
    middle = [r for t, done, r in rates if total * 0.1 <= done <= total * 0.9]
    if not middle:  # stopped early: the last third of the run
        middle = [r for t, done, r in rates if t >= end * 2 / 3]
    steady = median(middle) if middle else 0.0
    ramp = next((t for t, _, r in rates if steady and r >= steady * 0.8), None)
    dips, spell = 0, None
    for t, done, r in rates:
        inside = total * 0.05 <= done <= total * 0.95 and ttfb is not None and t - ttfb > WINDOW
        if inside and r < steady * DIP_RATIO:
            spell = t if spell is None else spell
            continue
        if spell is not None and t - spell >= DIP_SECONDS:
            dips += 1
        spell = None
    if spell is not None and end - spell >= DIP_SECONDS:
        dips += 1
    gap, last_t, last_done = 0.0, None, -1
    for t, done in samples:
        if done > last_done:
            if last_t is not None and last_done > 0:
                gap = max(gap, t - last_t)
            last_t, last_done = t, done
    gap = max(gap, end - last_t) if last_t is not None and last_done < total else gap
    t95, t99 = _first(samples, total, 0.95), _first(samples, total, 0.99)

    def r2(value):
        return None if value is None else round(value, 2)

    return {
        "seconds": r2(end),
        "ttfb": r2(ttfb),
        "ramp80": r2(ramp),
        "avg_mibps": round(samples[-1][1] / end / (1 << 20), 2) if end else 0.0,
        "steady_mibps": round(steady / (1 << 20), 2),
        "peak_mibps": round(max(r for *_, r in rates) / (1 << 20), 2),
        "dips": dips,
        "maxgap": r2(gap),
        "tail95": r2(end - t95) if t95 is not None else None,
        "tail99": r2(end - t99) if t99 is not None else None,
    }
