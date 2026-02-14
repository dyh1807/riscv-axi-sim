#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "${REPO_ROOT}"

git config core.hooksPath .githooks
echo "[ok] core.hooksPath set to .githooks"
