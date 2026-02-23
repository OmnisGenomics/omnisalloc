#!/bin/sh

set -e

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$script_dir/scripts/gen_run_tests.py" | bash
