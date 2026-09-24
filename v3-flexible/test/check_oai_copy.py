import sys
p, bwp_start, bwp_size, k0, msrs = sys.argv[1], *map(int, sys.argv[2:6])
rows = [l.strip().split(',') for l in open(p) if l[:1].isdigit()]
bad = []
for r in rows:
    rb, re = int(r[5]), int(r[6])
    # expected: average over the 12 subcarriers of RB rb of (value if inside SRS span else 0)
    lo, hi = bwp_start * 12 + k0, bwp_start * 12 + k0 + msrs      # SRS-covered carrier subcarriers [lo, hi)
    vals = [(g // 12 + 1) if lo <= g < hi else 0 for g in range(12 * rb, 12 * rb + 12)]
    if re != int(sum(vals) / 12): bad.append((rb, re, sum(vals) / 12))
print(f"rows {len(rows)} (bwp {bwp_start}+{bwp_size}), rb {rows[0][5]}..{rows[-1][5]}, mismatches {len(bad)} {bad[:3]}")
