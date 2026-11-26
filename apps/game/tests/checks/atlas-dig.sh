#!/usr/bin/env bash
# Digging on an atlas world, headless (sprint 30): every dig down through
# a shell floor timed against a budget (--probe atlas-dig: src/probe).
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build
probe_world terra "$probe_atlas_default"
probe_run atlas-dig 300 'ATLAS .*'
probe_zero_failed "atlas dig" 'ATLAS pass'
