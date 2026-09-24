# OAI CSI logging (SRS channel estimates, per RB)

Patches OpenAirInterface's gNB (`nr-softmodem`) so that every SRS channel estimate
computed by the PHY is written, per UE / RX antenna / SRS port / resource block,
to a CSV file inside the gNB container. A Streamlit application is provided to
analyse the CSV offline.

Current version: **logger v3.1**, **visualizer v8.7**.

## Repository layout

| Path | Content |
|---|---|
| `v3-flexible/src`, `v3-flexible/include` | Logger library (ring buffer, flush thread, CSV writer, env-var config) |
| `v3-flexible/patch_srs_ul_v3.sh` | Inserts the logger call in `nr_srs_rx_procedures()` (`phy_procedures_nr_gNB.c`) |
| `v3-flexible/apply_csi_v3_cmake.sh` | Links `libcsi_logging_v3.a` / json-c into `nr-softmodem` |
| `v3-flexible/patch_csi_init_main_v3.sh` | Adds the logger header to `nr-softmodem.c` |
| `v3-flexible/patch_oai_yaml_int8.sh` | Independent fix: yaml-cpp int8/uint8 misparse (RedCap r17, `ul_max_mcs`) |
| `v3-flexible/test/` | Host-side tests (no OAI build needed) |
| `docker/Dockerfile.build.csi.v3.rocky` | `ran-build-csi` image: patches + builds OAI with the logger |
| `docker/Dockerfile.gNB.csi.v3.rocky` | `oai-gnb-csi` image (rfsim / USRP) |
| `docker/Dockerfile.gNB.aw2s.csi.v3.rocky` | `oai-gnb-aw2s-csi` image |
| `visualizer/` | Streamlit analysis app + `requirements.txt` |

The directory name `v3-flexible/` and the Dockerfile names are used as-is by the
weekly GitLab CI (see below): do not rename them without updating the CI.

## How the images are built (weekly CI)

The CI clones `openairinterface5g` at tag `RAN_TAG` (e.g. `2026.w38`), then, from
this repository:

```bash
cp oai-csi-logging/docker/Dockerfile.build.csi.v3.rocky      docker/Dockerfile.build.csi.rocky
cp oai-csi-logging/docker/Dockerfile.gNB.csi.v3.rocky        docker/Dockerfile.gNB.csi.rocky
cp oai-csi-logging/docker/Dockerfile.gNB.aw2s.csi.v3.rocky   docker/Dockerfile.gNB.aw2s.csi.rocky
cp -r oai-csi-logging/v3-flexible .
cp oai-csi-logging/v3-flexible/{patch_srs_ul_v3.sh,apply_csi_v3_cmake.sh,patch_csi_init_main_v3.sh,patch_oai_yaml_int8.sh} .
docker build --target ran-build-csi    --tag ran-build-csi:latest       --file docker/Dockerfile.build.csi.rocky .
docker build --target oai-gnb-csi      --tag oai-gnb-csi:$RAN_TAG       --file docker/Dockerfile.gNB.csi.rocky .
docker build --target oai-gnb-aw2s-csi --tag oai-gnb-aw2s-csi:$RAN_TAG  --file docker/Dockerfile.gNB.aw2s.csi.rocky .
```

`Dockerfile.build.csi.v3.rocky` builds the library, copies the sources into
`openair1/PHY/NR_ESTIMATION/csi_v3_support/`, applies the patch scripts (each one
verifies its own result and fails the build otherwise) and builds OAI.

Source-level checks (patch anchors, variables in scope at the insertion point,
SRS buffer layout, compiler flags) were done on OAI tags 2026.w15, w20, w37 and
w38. Run-time validation must be redone on the testbed for each new tag.

## Enabling logging

The `-csi` images are selected by `oai5g-rru` when `CSI_ENABLED=true`
(`ansible-playbook ... -e csi_logger_enabled=true` in `5g_ansible`), which also
forces `do_SRS: periodic` and `do_CSIRS: 1` in the gNB configuration. The logger
starts at the first SRS it receives. Environment variables read by the logger:

| Variable | Default | Meaning |
|---|---|---|
| `CSI_OUTPUT_DIR` | `/data/csi` | Output directory; file `csi_per_rb.csv` (truncated at start) |
| `CSI_GRANULARITY` | `rb` | `rb` (average of the 12 subcarriers, complex) or `subcarrier` |
| `CSI_ANTENNA_SELECTION` | `all` | `all` or a comma list of RX antenna indices, e.g. `0,1` |
| `CSI_PORT_SELECTION` | `all` | `all` or a comma list of SRS port indices |
| `CSI_SUBCARRIER_SAMPLING` | `1` | Keep 1 subcarrier out of N (`subcarrier` granularity only) |
| `CSI_FLUSH_CORE` | `32` | CPU core the flush thread is pinned to |
| `CSI_INCLUDE_HEADER` | `true` | Write the JSON + column header |

## CSV format (v3.1)

```
# TIMESTAMP: 2026-09-24 08:09:26
# { "granularity": "rb", "nb_antenna_rx": 2, "nb_ports_tx": 1, ..., "format_version": "3.1",
#   "ofdm_symbol_size": 1536, "first_carrier_offset": 900, "bwp_start": 0, "bwp_size": 106,
#   "nb_symb_srs": 1, "srs_symbols": "averaged", "rb_index": "carrier CRB (DC-centred buffer offset applied)",
#   "timestamp": "UTC, taken at flush time = end of the batch that follows the marker",
#   "flush_period_s": 5, "columns": [ "frame", "slot", "rnti", "ant_rx", "port_tx", "rb", "real", "imag" ] }
frame,slot,rnti,ant_rx,port_tx,rb,real,imag
348,3,0xcb81,0,0,0,-812,415
...
# TIMESTAMP: 2026-09-24 08:09:31
...
```

- One row per (SRS occasion, RX antenna, SRS port, RB). `real`/`imag`: int16 channel
  estimate (OAI fixed point), averaged over the 12 subcarriers of the RB and over
  the SRS symbols. In `subcarrier` granularity a `sc` column (0..11) follows `rb`.
- `rb` is the carrier CRB index. RBs of the BWP outside the SRS bandwidth are 0.
- `frame` wraps every 1024 frames: use the timestamps for time axes.
- The JSON header and the column line are written once, at the first flush. The
  `columns` list is authoritative.
- A `# TIMESTAMP:` marker (UTC, 1 s resolution) is written at each flush (every
  5 s) **before** the rows buffered since the previous flush: rows after marker *k*
  were acquired between markers *k-1* and *k*.
- `# DROPPED: n` appears if the ring buffer (1M rows) was full; rows were lost.
- The number of RX antennas can differ between SRS PDUs (since OAI w37 it is
  `srs_parameters_v4.num_ul_spatial_streams_ports`); `nb_antenna_rx` in the JSON is
  the value of the first SRS.

### Files written by logger v3 (before v3.1)

Known defects, handled by visualizer v8.7 where possible:

- RB index missing the DC-centred buffer offset `first_carrier_offset - FFT/2`: the
  first RBs contain guard-band zeros and the top RBs are missing (11 RB for 106 PRB
  with `-E`, 18 RB + 10 subcarriers for aw2s 133 PRB). When the offset is not a
  multiple of 12, each logged RB mixes two real RBs and cannot be realigned. Use the
  guard-band correction of v8.7 (FFT size + carrier N_RB, see `Init: N_RB_DL ...,
  ofdm_symbol_size ...` in the gNB log).
- Only port 0 logged; JSON + header repeated at every flush; markers in the pod
  local time; single-antenna files have 6 columns (no `ant_rx,port_tx`).

## Visualizer

```bash
pip install -r visualizer/requirements.txt
streamlit run visualizer/streamlit_csi_visualizer_v8.7.py --server.maxUploadSize 4000
```

Upload a `csi_per_rb.csv` (v3 or v3.1). The whole file is loaded in memory:
parsing takes about 160 bytes per row (measured), i.e. ~2 GB of RAM for 10 M rows
plus the uploaded file itself.

## Tests

```bash
cd v3-flexible/test && ./run_tests.sh      # needs gcc, json-c-devel/libjson-c-dev, python3
```

- `test_layout`: SRS buffer with the OAI layout and RB-coded values; every logged
  row must carry the value of its RB/antenna/port (ASan/UBSan).
- `test_oai_copy`: buffer built with the verbatim OAI w37/w38 copy code, non-zero
  `bwp_start` and comb offset.
- `test_latency`: worst logger call duration while the flush thread writes.

## Changelog

- **v3.1** — Offset-corrected RB index (`first_carrier_offset` passed by the call
  site); multi-symbol SRS; all SRS ports (`N_ap`, was `1 << N_ap`); `all` antennas =
  antennas of the SRS buffer; header written once, always with `ant_rx,port_tx`,
  `sc` column declared; JSON geometry/format fields; UTC markers; CSV written
  outside the mutex shared with L1 (was ~30 ms of L1 blocking every 5 s); ring
  overflow counted; call-site prototype (the v3 anchor never matched, leaving an
  implicit declaration).
- **v3** — Flexible granularity / antenna / port selection, JSON metadata, flush
  thread. Older v1/v2 implementations (CSI-RS/SRS, `src/` at the repository root)
  were removed; they remain in the git history.
