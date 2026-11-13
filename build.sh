#!/bin/bash
set -euo pipefail

# Use existing PSPDEV if defined, otherwise check standard psp-config in PATH
if [ -z "${PSPDEV:-}" ]; then
    if command -v psp-config >/dev/null 2>&1; then
        export PSPDEV="$(psp-config --pspdev-path)"
    fi
fi

if [ -n "${PSPDEV:-}" ]; then
    export PATH="$PSPDEV/bin:$PATH"
fi

cd "$(dirname "$0")"

echo "Cleaning..."
make clean
echo "Building HCR PSP..."
make -j"$(nproc 2>/dev/null || echo 4)" "$@"
