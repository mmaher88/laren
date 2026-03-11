#!/bin/bash
# Thin wrapper: sources the canonical postinstall-common.sh for testing.
# The actual functions live in scripts/postinstall-common.sh (single source of truth).
_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../../.. && pwd)"
. "$_REPO_ROOT/scripts/postinstall-common.sh"
