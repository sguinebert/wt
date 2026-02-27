#!/usr/bin/env bash
set -euo pipefail

search_root="${1:-src/Wt/Dbo}"

pattern='dbo_error\{[A-Za-z_]+Error\{'

matches="$(rg -n "${pattern}" "${search_root}" --glob '!**/backend/**' || true)"

echo "Wt::Dbo legacy wrapper check"
echo "  pattern: ${pattern}"
echo "  root:    ${search_root}"

if [[ -n "${matches}" ]]; then
  echo "ERROR: legacy dbo_error wrapper constructors are still present:" >&2
  echo "${matches}" >&2
  exit 1
fi

echo "OK: no legacy dbo_error wrapper constructors detected."
