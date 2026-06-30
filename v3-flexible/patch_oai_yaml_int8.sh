#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# patch_oai_yaml_int8.sh
#
# Fixes the yaml-cpp int8/uint8 = "character" misparse in OAI's YAML config
# backend. yaml-cpp reads int8_t/uint8_t as a (signed/unsigned) char, so every
# TYPE_INT8 / TYPE_UINT8 parameter is silently dropped -- including
# cellBarredRedCap1Rx_r17 / cellBarredRedCap2Rx_r17 / intraFreqReselectionRedCap_r17
# (suppressed RedCap access in SIB1) and ul_max_mcs.
#
# Fix: read the scalar as 16-bit (a number, not a char); the existing
# assignment narrows it back to the int8 destination at runtime (plain
# assignment, NOT braced-init, so no -Wnarrowing).
#
# Designed for a weekly CI rebuild on a FRESH checkout:
#   * token-keyed, not line-number-keyed  -> survives upstream churn
#   * idempotent (only rewrites int8_t, leaves the int16_t result alone)
#   * FAILS LOUD if the read idiom is gone -> a refactor breaks the build
#     instead of silently shipping an unpatched parser.
#
# Usage (run BEFORE cmake, from the OAI checkout root or pass it as $1):
#   ./patch_oai_yaml_int8.sh [OAI_ROOT]
# ---------------------------------------------------------------------------
set -euo pipefail

OAI_ROOT="${1:-$PWD}"
SCOPE="$OAI_ROOT/common/config"

if [[ ! -d "$SCOPE" ]]; then
  echo "PATCH-FAIL: $SCOPE not found (wrong OAI_ROOT?)" >&2
  exit 1
fi

# Files that either still have an unpatched int8 read, OR already carry our
# marker. The marker keeps re-runs from false-failing once everything is fixed.
mapfile -t FILES < <(grep -rlE '\.as<[[:space:]]*u?int8_t[[:space:]]*>\(\)|/\*RC8\*/' "$SCOPE" 2>/dev/null || true)

if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "PATCH-FAIL: no '.as<int8_t>()/.as<uint8_t>()' read found under $SCOPE." >&2
  echo "            The yaml backend was likely refactored (templated as<T>?)." >&2
  echo "            Inspect:  grep -rn 'as<' $SCOPE/yaml/" >&2
  exit 1
fi

total=0
for F in "${FILES[@]}"; do
  # Only int8_t -> int16_t (and unsigned). Already-patched lines read int16_t
  # and are untouched, so this is naturally idempotent.
  sed -i -E \
    -e 's|\.as<[[:space:]]*int8_t[[:space:]]*>\(\)|.as<int16_t>() /*RC8*/|g' \
    -e 's|\.as<[[:space:]]*uint8_t[[:space:]]*>\(\)|.as<uint16_t>() /*RC8*/|g' \
    "$F"
  m=$(grep -c '/\*RC8\*/' "$F" || true)
  [[ "$m" -gt 0 ]] && echo "  patched $m read(s) in ${F#$OAI_ROOT/}"
  total=$(( total + m ))
done

if [[ "$total" -eq 0 ]]; then
  echo "PATCH-FAIL: 0 reads widened (token form changed)." >&2
  exit 1
fi

echo "OK: OAI yaml int8 parser patched ($total read(s) widened) -- RedCap r17 + ul_max_mcs now parse."
