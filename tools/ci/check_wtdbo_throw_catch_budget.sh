#!/usr/bin/env bash
set -euo pipefail

baseline_file="${1:-tools/ci/baselines/wtdbo_throw_catch_budget.env}"
search_root="${2:-src/Wt/Dbo}"

if [[ ! -f "${baseline_file}" ]]; then
  echo "ERROR: baseline file not found: ${baseline_file}" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "${baseline_file}"

: "${WTDBO_THROW_CATCH_TOTAL_BASELINE:?missing WTDBO_THROW_CATCH_TOTAL_BASELINE in baseline file}"
: "${WTDBO_THROW_BASELINE:?missing WTDBO_THROW_BASELINE in baseline file}"
: "${WTDBO_CATCH_BASELINE:?missing WTDBO_CATCH_BASELINE in baseline file}"

total_matches="$(rg -n "\\bthrow\\b|\\bcatch\\b" "${search_root}" --glob '!src/Wt/Dbo/third_party/**' | wc -l || true)"
throw_matches="$(rg -n "\\bthrow\\b" "${search_root}" --glob '!src/Wt/Dbo/third_party/**' | wc -l || true)"
catch_matches="$(rg -n "\\bcatch\\b" "${search_root}" --glob '!src/Wt/Dbo/third_party/**' | wc -l || true)"

echo "Wt::Dbo throw/catch budget check"
echo "  baseline total=${WTDBO_THROW_CATCH_TOTAL_BASELINE} throw=${WTDBO_THROW_BASELINE} catch=${WTDBO_CATCH_BASELINE}"
echo "  current  total=${total_matches} throw=${throw_matches} catch=${catch_matches}"

if (( total_matches > WTDBO_THROW_CATCH_TOTAL_BASELINE )); then
  echo "ERROR: throw/catch total increased above baseline." >&2
  exit 1
fi

if (( throw_matches > WTDBO_THROW_BASELINE )); then
  echo "ERROR: throw count increased above baseline." >&2
  exit 1
fi

if (( catch_matches > WTDBO_CATCH_BASELINE )); then
  echo "ERROR: catch count increased above baseline." >&2
  exit 1
fi

echo "OK: no throw/catch regression detected."
