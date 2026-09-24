import sys, json
path, NRB, nant, nap, nsym, gran = sys.argv[1], *map(int, sys.argv[2:6]), sys.argv[6]
bs = int(sys.argv[7]) if len(sys.argv) > 7 else 0; bz = int(sys.argv[8]) if len(sys.argv) > 8 else NRB
L = open(path).read().splitlines()
js = [json.loads(l[2:]) for l in L if l.startswith('# {')]; hdr = [l for l in L if l.startswith('frame')]
ts = [l for l in L if l.startswith('# TIMESTAMP')]
rows = [l.split(',') for l in L if l[:1].isdigit()]
assert len(js) == 1 and len(hdr) == 1, (len(js), len(hdr))
cols = js[0]['columns']; assert hdr[0].split(',') == cols
assert all(len(r) == len(cols) for r in rows)
d = [dict(zip(cols, r)) for r in rows]
bad = 0; seen = set()
for r in d:
    rb, a, p = int(r['rb']), int(r['ant_rx']), int(r['port_tx'])
    exp_r = rb + 1 + (nsym - 1); exp_q = 100 * a + 10 * p + 1 + (nsym - 1)
    if int(r['real']) != exp_r or int(r['imag']) != exp_q: bad += 1
    seen.add((int(r['frame']), rb, a, p, int(r.get('sc', 0))))
nsc = 12 if gran == 'subcarrier' else 1
expected = 3 * bz * nant * nap * nsc
print(f"rows {len(rows)} (expected {expected}), wrong values {bad}, rb {min(int(r['rb']) for r in d)}..{max(int(r['rb']) for r in d)}, "
      f"ports {sorted({r['port_tx'] for r in d})}, ants {sorted({r['ant_rx'] for r in d})}, markers {len(ts)}, "
      f"json fco={js[0]['first_carrier_offset']} N={js[0]['ofdm_symbol_size']} v={js[0]['format_version']}")
assert bad == 0 and len(rows) == expected and len(seen) == expected
