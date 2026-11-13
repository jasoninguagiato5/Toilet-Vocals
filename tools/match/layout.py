"""Shared layout of the matching test signal (times in seconds, levels in dBFS peak)."""
FS = 48000
TONE = 1000.0
LEAD = 1.0                                   # silence before the first tone

STAIR_LEVELS = list(range(-48, 1, 3))        # static curve: 17 steps, quiet to loud
STAIR_STEP = 1.5

GAP = 4.0                                    # silence so slow releases settle

# attack / release: quiet bed, a loud burst, quiet bed again
STEP_LOW, STEP_HIGH = -30.0, -6.0
STEP_PRE, STEP_LONG, STEP_SHORT, STEP_POST = 1.0, 1.5, 0.05, 4.0

THD_LEVELS = [-24.0, -18.0, -12.0, -6.0]     # harmonics at steady level
THD_STEP = 2.0


def sections():
    """Returns [(name, start, duration, level_db or None)] in order."""
    out, t = [], LEAD
    for lv in STAIR_LEVELS:
        out.append((f"stair{lv}", t, STAIR_STEP, float(lv))); t += STAIR_STEP
    t += GAP
    for tag, burst in (("long", STEP_LONG), ("short", STEP_SHORT)):
        out.append((f"{tag}_pre", t, STEP_PRE, STEP_LOW)); t += STEP_PRE
        out.append((f"{tag}_burst", t, burst, STEP_HIGH)); t += burst
        out.append((f"{tag}_post", t, STEP_POST, STEP_LOW)); t += STEP_POST
        t += GAP
    for lv in THD_LEVELS:
        out.append((f"thd{lv:g}", t, THD_STEP, lv)); t += THD_STEP
        t += 1.0
    return out, t + 1.0
