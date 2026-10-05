#!/usr/bin/env python3
"""Per-UE diagnosis of the frequency profile of SRS CSI (csi_per_rb.csv, OAI logger v3 / v3.1, OCUDU logger).

"subcarrier" granularity files (v3.1, column "sc") are reduced to RBs first: complex mean of the logged
subcarriers of each RB of each (occasion, antenna, port), as the "rb" granularity of the loggers.

Goal: find where a per-RB magnitude tilt comes from (UE TX chain, propagation, gNB/RU side,
power control / saturation), using magnitude AND phase of every SRS occasion.

  python3 csi_diag.py csi_per_rb.csv [--out diag] [--max-rows N]

Per SRS occasion (one UE, one RX antenna, one SRS port, all RBs of one SRS) it computes:
  level   mean |H| over the sounded RBs (dB)
  tilt    slope of 20log10|H| vs RB, linear fit (dB per 100 RB)
  ripple  std of the dB residual around that fit (dB)
  delay   group delay from the mean phase step between adjacent RBs (ns); RB spacing = 12 * SCS
Per UE it also computes the power delay profile (IFFT over the RBs after removing each occasion's
own delay), the correlation of the normalized dB profiles between UEs, and how tilt varies with
time and with level.

Output: summary on stdout, diag/summary.json and diag/*.png
"""
import argparse, calendar, json, os, sys, time
import warnings
import numpy as np
import pandas as pd
warnings.filterwarnings("ignore")

ap = argparse.ArgumentParser()
ap.add_argument("csv")
ap.add_argument("--out", default="diag")
ap.add_argument("--scs-khz", type=float, default=30.0)
ap.add_argument("--max-rows", type=int, default=0, help="read only the first N data rows (0 = all)")
ap.add_argument("--min-occasions", type=int, default=50)
ap.add_argument("--window", default="", help="t0:t1 (s from start): also plot level/tilt per UE over this window")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
RB_HZ = 12 * a.scs_khz * 1e3

# ---------------------------------------------------------------- pass 1: markers -> batch of each row
t0 = time.time()
batch_rows, batch_t, meta = [], [], {}
with open(a.csv, "rb") as f:
    n = 0
    for line in f:
        c = line[:1]
        if b"0" <= c <= b"9":
            if batch_rows:
                batch_rows[-1] += 1
            n += 1
            if a.max_rows and n >= a.max_rows:
                break
        elif line.startswith(b"# TIMESTAMP:"):
            try:
                batch_t.append(calendar.timegm(time.strptime(line[12:].strip().decode(), "%Y-%m-%d %H:%M:%S")))
            except ValueError:
                batch_t.append(np.nan)
            batch_rows.append(0)
        elif line.startswith(b"# {") and not meta:
            try:
                meta = json.loads(line[2:].decode())
            except ValueError:
                pass
cols = meta.get("columns") or ["frame", "slot", "rnti", "ant_rx", "port_tx", "rb", "real", "imag"]
print(f"[{time.time()-t0:5.1f}s] format {meta.get('format_version', 'v3 (pre-3.1)')}, {len(batch_t)} batches")

# ---------------------------------------------------------------- pass 2: rows
df = pd.read_csv(a.csv, comment="#", header=None, names=cols, low_memory=False,
                 nrows=a.max_rows or None, on_bad_lines="skip")
df = df[pd.to_numeric(df["frame"], errors="coerce").notna()]
for c in ("frame", "slot", "rb", "real", "imag") + (("ant_rx", "port_tx") if "ant_rx" in df else ()) + (("sc",) if "sc" in df else ()):
    df[c] = pd.to_numeric(df[c]).astype(np.float32 if c in ("real", "imag") else np.int32)
if "ant_rx" not in df:
    df["ant_rx"] = 0; df["port_tx"] = 0
rows_in_batches = int(np.sum(batch_rows))
lead = len(df) - rows_in_batches            # rows before the first marker (none with v3.1)
bidx = np.concatenate([np.full(max(lead, 0), -1), np.repeat(np.arange(len(batch_rows)), batch_rows)])[:len(df)]
df["batch"] = bidx
if "sc" in df:
    # subcarrier granularity: the logger writes the subcarriers of an RB on consecutive rows
    if df["sc"].min() < 0 or df["sc"].max() > 11:
        sys.exit(f"sc outside 0..11 (min {df['sc'].min()}, max {df['sc'].max()})")
    n_sc_rows, sc_vals = len(df), sorted(df["sc"].unique().tolist())
    kcols = ["frame", "slot", "rnti", "ant_rx", "port_tx", "rb"]
    first = np.flatnonzero((df[kcols] != df[kcols].shift()).any(axis=1).to_numpy())
    cnt = np.diff(np.append(first, len(df)))
    re_ = np.add.reduceat(df["real"].to_numpy(np.float64), first) / cnt
    im_ = np.add.reduceat(df["imag"].to_numpy(np.float64), first) / cnt
    df = df.iloc[first][kcols + ["batch"]].reset_index(drop=True)
    df["real"] = re_.astype(np.float32); df["imag"] = im_.astype(np.float32)
    print(f"[{time.time()-t0:5.1f}s] granularity subcarrier (sc {sc_vals}, subcarrier_sampling "
          f"{meta.get('subcarrier_sampling', '?')}): {n_sc_rows:,} rows -> {len(df):,} RB rows (complex mean)")
print(f"[{time.time()-t0:5.1f}s] {len(df):,} rows, RNTIs {sorted(df.rnti.unique())}")

# ---------------------------------------------------------------- occasions (one block = one SRS, one ant, one port)
key = df[["frame", "slot", "rnti", "ant_rx", "port_tx"]]
new = (key != key.shift()).any(axis=1).to_numpy() | (np.diff(df["rb"].to_numpy(), prepend=10**9) <= 0)
df["occ"] = np.cumsum(new) - 1
n_rb = int(df["rb"].max()) + 1
nocc = int(df["occ"].iloc[-1]) + 1
H = np.zeros((nocc, n_rb), np.complex64)
H[df["occ"].to_numpy(), df["rb"].to_numpy()] = df["real"].to_numpy() + 1j * df["imag"].to_numpy()
occ = df.groupby("occ").agg(rnti=("rnti", "first"), ant=("ant_rx", "first"), port=("port_tx", "first"),
                            batch=("batch", "first")).reset_index(drop=True)
mag = np.abs(H)
sounded = (mag > 0).mean(axis=0) > 0.5     # RBs that carry SRS in most occasions
rbs = np.flatnonzero(sounded)
print(f"[{time.time()-t0:5.1f}s] {nocc:,} occasions, {n_rb} RB, sounded RB {rbs.min()}..{rbs.max()} ({len(rbs)})")

Hs = H[:, rbs]; Ms = mag[:, rbs]
ok = (Ms > 0).all(axis=1)
db = 20 * np.log10(np.where(Ms > 0, Ms, np.nan))
x = (rbs - rbs.mean()).astype(np.float64)
level = 20 * np.log10(np.nanmean(Ms, axis=1) + 1e-9)
slope = np.nansum((db - np.nanmean(db, axis=1, keepdims=True)) * x, axis=1) / np.sum(x * x)   # dB per RB
fit = np.nanmean(db, axis=1, keepdims=True) + slope[:, None] * x
ripple = np.nanstd(db - fit, axis=1)
step = np.angle(np.sum(Hs[:, 1:] * np.conj(Hs[:, :-1]), axis=1))       # mean phase step per RB (rad)
delay_ns = -step / (2 * np.pi * RB_HZ) * 1e9
occ = occ.assign(level_db=level, tilt=slope * 100, ripple_db=ripple, delay_ns=delay_ns, ok=ok)
occ["t"] = [batch_t[b] if 0 <= b < len(batch_t) else np.nan for b in occ["batch"]]
occ.to_csv(os.path.join(a.out, "occasions.csv.gz"), index=False, float_format="%.3f")   # one row per SRS occasion

# ---------------------------------------------------------------- per UE (and antenna, port)
summary = {"file": a.csv, "format": meta.get("format_version", "v3"), "granularity": meta.get("granularity", "rb"),
           "source": meta.get("source", "oai"), "rows": int(len(df)), "occasions": nocc,
           "sounded_rb": [int(rbs.min()), int(rbs.max())], "groups": []}
groups = [g for g in occ[occ.ok].groupby(["rnti", "ant", "port"]) if len(g[1]) >= a.min_occasions]
L = 512
prof, pdp = {}, {}
for (r, an, po), g in groups:
    i = g.index.to_numpy()
    # mean dB profile (normalized), PDP after removing each occasion's own delay
    p = np.nanmean(db[i], axis=0); prof[(r, an, po)] = p - np.nanmean(p)
    ph = np.exp(-1j * step[i][:, None] * np.arange(len(rbs))[None, :])
    hn = Hs[i] * ph / (np.abs(Hs[i]).mean(axis=1, keepdims=True) + 1e-9)
    win = np.hanning(len(rbs))[None, :]      # Hann window: sidelobes < -31 dB (a flat channel shows no 2nd peak)
    P = (np.abs(np.fft.ifft(hn * win, n=L, axis=1)) ** 2).mean(axis=0)
    P = np.roll(P, L // 4); pdp[(r, an, po)] = 10 * np.log10(P / P.max() + 1e-12)
    # secondary peak in the PDP (outside the main lobe)
    k0 = int(np.argmax(P)); res = len(rbs)
    side = P.copy(); lob = max(3, 3 * L // res)   # Hann main lobe = +-2 resolution cells
    side[max(0, k0 - lob):k0 + lob + 1] = 0
    k1 = int(np.argmax(side))
    tau_bin = 1e9 / (RB_HZ * L)
    corr = lambda u, v: float(pd.Series(u).corr(pd.Series(v), method="spearman"))
    tb = g.dropna(subset=["t"]).groupby("t").agg(tilt=("tilt", "median"), level=("level_db", "median"))
    summary["groups"].append({
        "rnti": r, "ant": int(an), "port": int(po), "occasions": int(len(g)),
        "level_db_median": round(float(g.level_db.median()), 2), "level_db_iqr": round(float(g.level_db.quantile(.75) - g.level_db.quantile(.25)), 2),
        "tilt_db_per_100rb_median": round(float(g.tilt.median()), 2),
        "tilt_iqr": round(float(g.tilt.quantile(.75) - g.tilt.quantile(.25)), 2),
        "ripple_db_median": round(float(g.ripple_db.median()), 2),
        "delay_ns_median": round(float(g.delay_ns.median()), 1), "delay_ns_std": round(float(g.delay_ns.std()), 1),
        "pdp_second_peak_db": round(float(10 * np.log10(side[k1] / P[k0] + 1e-12)), 1),
        "pdp_second_peak_ns": round(float((k1 - k0) * tau_bin), 1),
        "spearman_tilt_vs_level_occ": round(corr(g.tilt, g.level_db), 2),
        "spearman_tilt_vs_delay_occ": round(corr(g.tilt, g.delay_ns), 2),
        "spearman_tilt_vs_level_batch": round(corr(tb.tilt, tb.level), 2) if len(tb) > 5 else None,
        "tilt_first_10pct": round(float(g.tilt.iloc[:max(1, len(g)//10)].median()), 2),
        "tilt_last_10pct": round(float(g.tilt.iloc[-max(1, len(g)//10):].median()), 2),
    })
keys = list(prof)
# per UE and per 5 s batch: mean |H| and mean dB per RB, to study events offline without re-reading the CSV
dump = {"rbs": rbs, "batch_t": np.array(batch_t, dtype=float)}
for k in keys:
    g = occ[(occ.rnti == k[0]) & (occ.ant == k[1]) & (occ.port == k[2]) & occ.ok]
    bt = g.batch.to_numpy(); ub = np.unique(bt[bt >= 0])
    lin = np.vstack([Ms[g.index[bt == b]].mean(axis=0) for b in ub])
    ddb = np.vstack([np.nanmean(db[g.index[bt == b]], axis=0) for b in ub])
    tag = f"{k[0]}_a{k[1]}_p{k[2]}"
    dump[tag + "_batches"] = ub; dump[tag + "_lin"] = lin.astype(np.float32); dump[tag + "_db"] = ddb.astype(np.float32)
    dump[tag + "_n"] = np.array([(bt == b).sum() for b in ub])
np.savez_compressed(os.path.join(a.out, "batch_profiles.npz"), **dump)
C = np.array([[np.corrcoef(prof[u], prof[v])[0, 1] for v in keys] for u in keys]) if keys else np.zeros((0, 0))
summary["profile_correlation"] = {"keys": [f"{k[0]}/a{k[1]}/p{k[2]}" for k in keys], "matrix": np.round(C, 2).tolist()}
# same after removing each profile's own linear trend: is the RIPPLE common to all UEs?
xr = rbs - rbs.mean()
detr = {k: prof[k] - np.polyval(np.polyfit(xr, prof[k], 1), xr) for k in keys}
Cr = np.array([[np.corrcoef(detr[u], detr[v])[0, 1] for v in keys] for u in keys]) if keys else np.zeros((0, 0))
summary["ripple_correlation"] = np.round(Cr, 2).tolist()
summary["zero_rb_occasions_pct"] = round(float(100 * (~ok).mean()), 2)
json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=2)

print("\nrnti/ant/port   n_occ  level_dB(IQR)  tilt_dB/100RB(IQR)  ripple_dB  delay_ns(std)  PDP_2nd(dB@ns)  rho(tilt,level) occ/batch  rho(tilt,delay)  tilt first/last")
for s in summary["groups"]:
    print(f"{s['rnti']}/a{s['ant']}/p{s['port']}  {s['occasions']:7d}  {s['level_db_median']:6.1f}({s['level_db_iqr']:4.1f})"
          f"   {s['tilt_db_per_100rb_median']:6.2f}({s['tilt_iqr']:4.2f})      {s['ripple_db_median']:5.2f}"
          f"   {s['delay_ns_median']:7.1f}({s['delay_ns_std']:5.1f})   {s['pdp_second_peak_db']:6.1f}@{s['pdp_second_peak_ns']:6.1f}"
          f"     {s['spearman_tilt_vs_level_occ']:5.2f}/{s['spearman_tilt_vs_level_batch']}      {s['spearman_tilt_vs_delay_occ']:5.2f}"
          f"      {s['tilt_first_10pct']:5.2f}/{s['tilt_last_10pct']:5.2f}")
print(f"\noccasions with a zero sounded RB (excluded; e.g. ant/port pairs without signal): {summary['zero_rb_occasions_pct']} %")
print("correlation of the normalized dB profiles between groups (shape, tilt included):")
for k, row in zip(summary["profile_correlation"]["keys"], summary["profile_correlation"]["matrix"]):
    print(f"  {k:16s} " + " ".join(f"{v:5.2f}" for v in row))
print("correlation of the profiles after removing each one's linear trend (ripple only):")
for k, row in zip(summary["profile_correlation"]["keys"], summary["ripple_correlation"]):
    print(f"  {k:16s} " + " ".join(f"{v:5.2f}" for v in row))

# ---------------------------------------------------------------- figures
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
fig, ax = plt.subplots(2, 2, figsize=(15, 10))
for k in keys:
    lab = f"{k[0]} a{k[1]}"
    ax[0, 0].plot(rbs, prof[k], label=lab)
    g = occ[(occ.rnti == k[0]) & (occ.ant == k[1]) & (occ.port == k[2]) & occ.ok]
    ax[0, 1].plot(g.t - np.nanmin(occ.t), g.tilt.rolling(200, min_periods=20).median(), label=lab)
    ax[1, 0].plot((np.arange(L) - L // 4) * 1e9 / (RB_HZ * L), pdp[k], label=lab)
    s = g.sample(min(len(g), 3000), random_state=0)
    ax[1, 1].scatter(s.level_db, s.tilt, s=3, alpha=.3, label=lab)
ax[0, 0].set(title="Mean dB profile per UE (mean removed)", xlabel="RB", ylabel="dB")
ax[0, 1].set(title="Tilt over time (rolling median, 200 occasions)", xlabel="s from start", ylabel="dB / 100 RB")
ax[1, 0].set(title="Power delay profile (own delay removed)", xlabel="ns", ylabel="dB", xlim=(-200, 1500), ylim=(-40, 1))
ax[1, 1].set(title="Tilt vs level, per occasion", xlabel="level (dB a.u.)", ylabel="dB / 100 RB")
for x_ in ax.flat:
    x_.grid(alpha=.3); x_.legend(fontsize=8)
plt.tight_layout(); plt.savefig(os.path.join(a.out, "diag.png"), dpi=110)
if a.window:
    w0, w1 = (float(v) for v in a.window.split(":"))
    tt = occ.t - np.nanmin(occ.t)
    fig, ax = plt.subplots(3, 1, figsize=(14, 10), sharex=True)
    for k in keys:
        g = occ[(occ.rnti == k[0]) & (occ.ant == k[1]) & (occ.port == k[2]) & occ.ok & (tt >= w0) & (tt <= w1)]
        x_ = tt[g.index]
        ax[0].plot(x_, g.level_db, ".", ms=2, label=k[0]); ax[1].plot(x_, g.tilt, ".", ms=2, label=k[0])
        ax[2].plot(x_, g.ripple_db, ".", ms=2, label=k[0])
    ax[0].set_ylabel("level (dB a.u.)"); ax[1].set_ylabel("tilt (dB / 100 RB)"); ax[2].set_ylabel("ripple (dB)")
    ax[2].set_xlabel("s from start (batch resolution: 5 s)")
    for x_ in ax: x_.grid(alpha=.3); x_.legend(fontsize=8, markerscale=4)
    plt.tight_layout(); plt.savefig(os.path.join(a.out, "window.png"), dpi=110)
    sub = occ[occ.ok & (tt >= w0) & (tt <= w1)].assign(ts=(tt // 5) * 5)
    print("\nper 5 s batch in the window: median level_dB / tilt per UE")
    print(sub.pivot_table(index="ts", columns="rnti", values=["level_db", "tilt"], aggfunc="median").astype(float).round(2).to_string())
print(f"\n[{time.time()-t0:5.1f}s] wrote {a.out}/summary.json and {a.out}/diag.png")
